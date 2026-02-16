#include "SipService.h"
#include "CallService.h"
#include "HardwareService.h"
#include "StorageService.h"
#include <QDebug>
#include <pjsua-lib/pjsua.h>
#include <pjsua2.hpp>

using namespace pj;
#include <QSet>

static QSet<QString> g_activeStunServers;

SipService &SipService::instance() {
  static SipService _instance;
  return _instance;
}

SipService::SipService(QObject *parent)
    : QObject(parent), m_endpoint(nullptr) {}

SipService::~SipService() { shutdown(); }

void SipService::init() {
  try {
    m_endpoint = new Endpoint;
    m_endpoint->libCreate();

    EpConfig ep_cfg;
    ep_cfg.logConfig.level = 4;

    // Set max calls before libInit
    ep_cfg.uaConfig.maxCalls = 4;

    // Force X-nat header in SDP (always enabled as requested)
    ep_cfg.uaConfig.natTypeInSdp = 1;

    // Mimic MicroSIP User Agent to ensure compatibility with strict PBX rules
    ep_cfg.uaConfig.userAgent = "MicroSIP/3.21.3";

    // Disable automatic switching from UDP to TCP
    pjsip_cfg()->endpt.disable_tcp_switch = PJ_TRUE;

    // Try to initialize with default audio, but catch errors
    bool audioInitFailed = false;
    try {
      m_endpoint->libInit(ep_cfg);
      qDebug() << "PJSIP library initialized with audio";
    } catch (Error &err) {
      if (err.status == 420002) { // PJMEDIA_EAUD_SYSERR
        qWarning() << "Audio device initialization failed, recreating endpoint "
                      "with null device";
        audioInitFailed = true;

        // Destroy the failed endpoint and create a new one
        delete m_endpoint;
        m_endpoint = new Endpoint;
        m_endpoint->libCreate();

        // Configure media with proper settings for null device
        ep_cfg.medConfig.noVad = true;
        ep_cfg.medConfig.clockRate = 16000; // 16kHz is standard for narrowband
        ep_cfg.medConfig.channelCount = 1;
        ep_cfg.medConfig.audioFramePtime = 20;
        ep_cfg.medConfig.maxMediaPorts = 32;

        // Try again with new endpoint
        m_endpoint->libInit(ep_cfg);
        qDebug() << "PJSIP library initialized without audio";
      } else {
        throw;
      }
    }

    HardwareService::instance().init(m_endpoint);

    TransportConfig t_cfg;
    t_cfg.port = 5060;
    try {
      m_udpTransportId =
          m_endpoint->transportCreate(PJSIP_TRANSPORT_UDP, t_cfg);
    } catch (Error &) {
      qWarning() << "Port 5060 occupied, trying auto-assign...";
      t_cfg.port = 0;
      m_udpTransportId =
          m_endpoint->transportCreate(PJSIP_TRANSPORT_UDP, t_cfg);
    }

    // Attempt TCP on the same port or auto-assign if UDP was auto-assigned
    try {
      m_tcpTransportId =
          m_endpoint->transportCreate(PJSIP_TRANSPORT_TCP, t_cfg);
    } catch (Error &err) {
      qWarning() << "Could not create TCP transport:"
                 << QString::fromStdString(err.info());
    }

    m_endpoint->libStart();
    qDebug() << "PJSIP started";

    // Configure null sound device if audio init failed
    if (audioInitFailed) {
      try {
        m_endpoint->audDevManager().setNullDev();
        qDebug() << "Null sound device configured via audDevManager";
      } catch (Error &err) {
        qWarning() << "Could not set null sound device:"
                   << QString::fromStdString(err.info());
      }
    }

    qDebug() << "PJSIP initialized successfully";

    try {
      const CodecInfoVector2 &codecs = m_endpoint->codecEnum2();
      qDebug() << "Available Codecs (Configuring...):";
      for (unsigned i = 0; i < codecs.size(); ++i) {
        QString codecId = QString::fromStdString(codecs[i].codecId);
        bool priority = 0;
        if (codecId.contains("PCMU") || codecId.contains("PCMA") ||
            codecId.contains("G722") || codecId.contains("opus") ||
            codecId.contains("telephone-event")) {
          priority = 128 + i; // Give some priority
        } else {
          priority = 0; // Disable others to save SDP space
        }
        m_endpoint->codecSetPriority(codecs[i].codecId, priority);
        qDebug() << " - " << codecId << "priority:" << priority;
      }
    } catch (Error &err) {
      qWarning() << "Error configuring codecs:"
                 << QString::fromStdString(err.info());
    }

    emit initStatusChanged(true, "Initialized");

  } catch (Error &err) {
    qCritical() << "PJSIP Initialization error:"
                << QString::fromStdString(err.info());

    // If we have an endpoint but just one step failed, we might still be able
    // to show devices
    if (m_endpoint) {
      HardwareService::instance().init(m_endpoint);
    }

    emit initStatusChanged(false, QString::fromStdString(err.info()));
  }
}

void SipService::shutdown() {
  if (m_endpoint) {
    try {
      m_accounts.clear(); // Ensure accounts are deleted before endpoint
      m_endpoint->libDestroy();
    } catch (Error &err) {
      qWarning() << "Error destroying PJSIP lib:"
                 << QString::fromStdString(err.info());
    }
    delete m_endpoint;
    m_endpoint = nullptr;
  }
}

void SipService::registerAccount(const QString &id, const QVariantMap &config) {
  if (!m_endpoint)
    return;

  try {
    if (m_accounts.contains(id)) {
      // Already exists, maybe update? For now remove and recreate
      m_accounts.remove(id);
    }

    AccountConfig cfg;
    QString username = config["username"].toString();
    QString domain = config["domain"].toString();
    QString password = config["password"].toString();
    QString proxy = config["proxy"].toString();

    QString transport = config["transport"].toString().toLower();
    std::string transportParam = "";
    if (transport == "tcp") {
      transportParam = ";transport=tcp";
      cfg.sipConfig.transportId = m_tcpTransportId;
    } else if (transport == "tls") {
      transportParam = ";transport=tls";
    } else if (transport == "udp") {
      transportParam = ";transport=udp";
      if (config["rport"].toBool()) {
        transportParam += ";rport";
      }
      cfg.sipConfig.transportId = m_udpTransportId;
    }

    cfg.idUri = "<sip:" + (username + "@" + domain).toStdString() + ">";
    cfg.regConfig.registrarUri = "sip:" + domain.toStdString() + transportParam;

    if (!proxy.isEmpty()) {
      cfg.sipConfig.proxies.push_back("sip:" + proxy.toStdString() +
                                      transportParam);
    }

    QString login = config["login"].toString();
    if (login.isEmpty())
      login = username;

    cfg.sipConfig.authCreds.push_back(AuthCredInfo(
        "digest", "*", login.toStdString(), 0, password.toStdString()));

    // NAT Traversal
    bool autoNat = config["autoNat"].toBool();
    bool allowIpRewrite = config["allowIpRewrite"].toBool();

    if (autoNat) {
      cfg.natConfig.contactRewriteUse = PJ_TRUE;
      cfg.natConfig.viaRewriteUse = PJ_TRUE;
      cfg.natConfig.sdpNatRewriteUse = PJ_TRUE;
    } else {
      cfg.natConfig.contactRewriteUse = allowIpRewrite ? PJ_TRUE : PJ_FALSE;
      cfg.natConfig.viaRewriteUse = PJ_FALSE;
      // If IP rewrite is allowed, we should also rewrite SDP to use the public
      // IP instead of local IP (fixes 127.0.0.1 in c= line)
      cfg.natConfig.sdpNatRewriteUse = allowIpRewrite ? PJ_TRUE : PJ_FALSE;
    }
    cfg.natConfig.iceEnabled = config["ice"].toBool() ? PJ_TRUE : PJ_FALSE;

    bool enableStun = config["enableStun"].toBool();
    cfg.natConfig.sipStunUse =
        enableStun ? PJSUA_STUN_USE_DEFAULT : PJSUA_STUN_USE_DISABLED;
    cfg.natConfig.mediaStunUse =
        enableStun ? PJSUA_STUN_USE_DEFAULT : PJSUA_STUN_USE_DISABLED;

    // Rebuild global STUN servers from all active accounts
    QSet<QString> activeStunServers;
    QList<AccountRecord> records = StorageService::instance().getAccounts();
    for (const auto &rec : records) {
      if (rec.active && rec.enableStun) {
        if (rec.stunServer.isEmpty()) {
          activeStunServers.insert("stun.l.google.com:19302");
        } else {
          activeStunServers.insert(rec.stunServer);
        }
      }
    }

    // Apply to PJSIP
    std::vector<std::string> stunStdStrs;
    for (const QString &s : activeStunServers) {
      stunStdStrs.push_back(s.toStdString());
    }

    // We must pass at least one server or empty list?
    // If empty list, PJSIP will have no STUN servers.

    pj_str_t *servers = nullptr;
    if (!stunStdStrs.empty()) {
      servers = new pj_str_t[stunStdStrs.size()];
      for (size_t i = 0; i < stunStdStrs.size(); ++i) {
        servers[i] = pj_str(const_cast<char *>(stunStdStrs[i].c_str()));
      }

      pj_status_t status =
          pjsua_update_stun_servers(stunStdStrs.size(), servers, PJ_FALSE);
      if (status != PJ_SUCCESS) {
        qWarning() << "Failed to update global STUN servers:" << status;
      } else {
        qDebug() << "Updated global STUN servers, count:" << stunStdStrs.size();
      }
    } else {
      // PJSIP crashes if we call update_stun_servers with 0 servers.
      // We cannot clear the list dynamically with this API version.
      // For now, allow it to remain as is (stale servers might persist if all
      // accounts removed). Since we start with 0 servers in init(), this is
      // only an issue if we add then remove all.
      qDebug() << "No STUN servers to configure (skipping update to avoid "
                  "assertion)";
    }

    if (servers)
      delete[] servers;

    QString turn = config["turnServer"].toString();
    if (!turn.isEmpty()) {
      cfg.natConfig.turnEnabled = PJ_TRUE;
      cfg.natConfig.turnServer = turn.toStdString();
      cfg.natConfig.turnConnType = PJ_TURN_TP_UDP;
      cfg.natConfig.turnUserName = config["turnUser"].toString().toStdString();
      cfg.natConfig.turnPassword = config["turnPass"].toString().toStdString();
      cfg.natConfig.turnPasswordType = 0;
    }

    // Security (SRTP)
    // 0:Disabled, 1:Optional, 2:Mandatory
    cfg.mediaConfig.srtpUse = (pjmedia_srtp_use)config["srtpMode"].toInt();
    cfg.mediaConfig.srtpSecureSignaling = 0;

    // IP Version (0:IPv4, 1:IPv6, 2:Auto)
    int ipVersion = config["ipVersion"].toInt();
    if (ipVersion == 0) { // IPv4 Only
      cfg.sipConfig.ipv6Use = PJSUA_IPV6_DISABLED;
      cfg.mediaConfig.ipv6Use = PJSUA_IPV6_DISABLED;
    } else if (ipVersion == 1) { // IPv6 Only
      cfg.sipConfig.ipv6Use = PJSUA_IPV6_ENABLED;
      cfg.mediaConfig.ipv6Use = PJSUA_IPV6_ENABLED;
    } else { // Auto (2)
      // Default pjsip behavior usually prefers IPv4 for media in dual stack
      // unless configured otherwise
      cfg.mediaConfig.ipv6Use = PJSUA_IPV6_ENABLED;
    }

    auto acc = std::make_shared<MyAccount>();
    acc->create(cfg);
    m_accounts[id] = acc;

    qDebug() << "Registered account:" << id;

  } catch (Error &err) {
    qCritical() << "Error registering account:"
                << QString::fromStdString(err.info());
    emit accountStateChanged(id, false, 500,
                             QString::fromStdString(err.info()));
  }
}

void SipService::unregisterAccount(const QString &id) {
  if (m_accounts.contains(id)) {
    m_accounts.remove(
        id); // Destructor of MyAccount (if managed properly) should unregister?
    // pj::Account destructor calls shutdown usually?
    // Actually we should explicitly set registration to false if we want to
    // keep object? For now, removing from map destroys the object.
    qDebug() << "Unregistered account:" << id;
    emit accountStateChanged(id, false, 0, "Unregistered");
  }
}

void MyAccount::onRegState(pj::OnRegStateParam &prm) {
  AccountInfo ai = getInfo();
  qDebug() << (ai.regIsActive ? "*** Register:" : "*** Unregister:")
           << " code=" << prm.code;

  // We need a way to signal back to SipService or UI
  // SipService::instance().emit... (using singleton for now)
  SipService::instance().accountStateChanged(
      QString::number(getId()), ai.regIsActive, prm.code,
      QString::fromStdString(prm.reason));
}
void MyAccount::onIncomingCall(pj::OnIncomingCallParam &prm) {
  auto call = std::make_shared<MyCall>(*this, prm.callId);
  CallService::instance().addCall(call);

  CallInfo ci = call->getInfo();
  CallService::instance().incomingCall(prm.callId,
                                       QString::fromStdString(ci.remoteUri));
}

void MyAccount::onInstantMessage(pj::OnInstantMessageParam &prm) {
  QString remoteUri = QString::fromStdString(prm.fromUri);
  QString content = QString::fromStdString(prm.msgBody);

  qDebug() << "Incoming message from" << remoteUri << ":" << content;

  // Save to database
  StorageService::instance().addMessage(remoteUri, "Incoming", content);

  // We might want to signal MessageModel to refresh if it's viewing this
  // contact Or use a global MessageService?
}

void MyAccount::onInstantMessageStatus(pj::OnInstantMessageStatusParam &prm) {
  qDebug() << "IM Status:" << QString::fromStdString(prm.reason);
}

void MyAccount::onTypingIndication(pj::OnTypingIndicationParam &prm) {
  QString fromUri = QString::fromStdString(prm.fromUri);
  qDebug() << "Typing indication from" << fromUri << ":"
           << (prm.isTyping ? "Typing..." : "Stopped");

  emit SipService::instance().typingIndicationReceived(fromUri, prm.isTyping);
}

void SipService::sendMessage(const QString &accountId,
                             const QString &destination,
                             const QString &content) {
  auto acc = getAccount(accountId);
  if (!acc) {
    qWarning() << "Account not found for sending message:" << accountId;
    return;
  }

  try {
    std::string destStr = destination.toStdString();
    std::string contentStr = content.toStdString();
    pj_str_t pj_dest = pj_str((char *)destStr.c_str());
    pj_str_t pj_content = pj_str((char *)contentStr.c_str());

    pj_status_t status = pjsua_im_send(acc->getId(), &pj_dest, nullptr,
                                       &pj_content, nullptr, nullptr);
    if (status != PJ_SUCCESS) {
      qWarning() << "Error sending SIP message:" << status;
    }

    // Save to database
    StorageService::instance().addMessage(destination, "Outgoing", content);
  } catch (const std::exception &e) {
    qWarning() << "Exception sending message:" << e.what();
  }
}
