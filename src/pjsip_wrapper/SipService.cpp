#include "SipService.h"
#include "BlfService.h"
#include "BuddyService.h"
#include "CallService.h"
#include "HardwareService.h"
#include "StorageService.h"
#include "SipUri.h"
#include <QDebug>
#include <QFile>
#include <QNetworkInformation>
#include <QSettings>
#include <pjsua-lib/pjsua.h>
#include <functional>
#include <future>
#include <pjsua2.hpp>

using namespace pj;
#include <QSet>

SipService &SipService::instance() {
  static SipService _instance;
  return _instance;
}

SipService::SipService(QObject *parent)
    : QObject(parent), m_endpoint(nullptr) {
  QSettings settings;
  m_doNotDisturb = settings.value("calls/doNotDisturb", false).toBool();
  m_autoAnswer = settings.value("calls/autoAnswer", false).toBool();
}

void SipService::setDoNotDisturb(bool enabled) {
  if (m_doNotDisturb.exchange(enabled) == enabled)
    return;
  QSettings().setValue("calls/doNotDisturb", enabled);
  emit callHandlingChanged();
}

void SipService::setAutoAnswer(bool enabled) {
  if (m_autoAnswer.exchange(enabled) == enabled)
    return;
  QSettings().setValue("calls/autoAnswer", enabled);
  emit callHandlingChanged();
}

bool SipService::tlsVerify() const {
  return QSettings().value("sip/tlsVerify", true).toBool();
}

void SipService::setTlsVerify(bool enabled) {
  if (tlsVerify() == enabled)
    return;
  QSettings().setValue("sip/tlsVerify", enabled);
  emit callHandlingChanged();
}

// Runs `fn` on the PJSIP worker thread and waits for it.
//
// Deleting an account takes PJSIP's global lock and then the lock of each
// presence dialog. The worker thread takes them the other way round when a
// SUBSCRIBE arrives, so deleting from the GUI thread can deadlock. Done on the
// worker itself there is nobody left to take them in the other order.
static void runOnSipThread(std::function<void()> fn) {
  struct Job {
    std::function<void()> fn;
    std::promise<void> done;
  } job{std::move(fn), {}};
  void (*trampoline)(void *) = [](void *p) {
    Job *j = static_cast<Job *>(p);
    j->fn();
    j->done.set_value();
  };
  if (pjsua_get_state() != PJSUA_STATE_RUNNING ||
      pjsua_schedule_timer2(trampoline, &job, 0) != PJ_SUCCESS) {
    job.fn();
    return;
  }
  job.done.get_future().wait();
}

SipService::~SipService() { shutdown(); }

void SipService::init() {
  try {
    m_endpoint = new Endpoint;
    m_endpoint->libCreate();

    EpConfig ep_cfg;
    // Full SIP traces contain Authorization headers, so they are opt-in:
    // run with MICROSIP_SIP_TRACE=1 when debugging signalling.
    const bool trace = qEnvironmentVariableIsSet("MICROSIP_SIP_TRACE");
    ep_cfg.logConfig.level = trace ? 5 : 3;
    ep_cfg.logConfig.consoleLevel = trace ? 5 : 3;
    ep_cfg.logConfig.msgLogging = trace;

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

    // IPv6 signalling, on its own port; absent where the host has no IPv6
    try {
      TransportConfig v6_cfg;
      m_udp6TransportId =
          m_endpoint->transportCreate(PJSIP_TRANSPORT_UDP6, v6_cfg);
    } catch (Error &) {
      qInfo() << "No IPv6 UDP transport available";
    }

    // Attempt TCP on the same port or auto-assign if UDP was auto-assigned
    try {
      m_tcpTransportId =
          m_endpoint->transportCreate(PJSIP_TRANSPORT_TCP, t_cfg);
    } catch (Error &err) {
      qWarning() << "Could not create TCP transport:"
                 << QString::fromStdString(err.info());
    }

    // Client-only TLS on an ephemeral port. The server certificate is checked
    // against the system CA bundle unless the user turned that off (a PBX
    // with a self-signed certificate needs it off).
    try {
      TransportConfig tls_cfg;
      if (tlsVerify()) {
        tls_cfg.tlsConfig.verifyServer = true;
        for (const char *bundle : {"/etc/ssl/certs/ca-certificates.crt",
                                   "/etc/pki/tls/certs/ca-bundle.crt",
                                   "/etc/ssl/cert.pem"}) {
          if (QFile::exists(bundle)) {
            tls_cfg.tlsConfig.CaListFile = bundle;
            break;
          }
        }
      }
      m_tlsTransportId =
          m_endpoint->transportCreate(PJSIP_TRANSPORT_TLS, tls_cfg);
    } catch (Error &err) {
      qWarning() << "Could not create TLS transport:"
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

    HardwareService::instance().applyCodecPriorities();

    // Local account: no registrar, identified by our own address. Direct IP
    // calls go out through it, and calls addressed to our IP come in on it.
    try {
      pjsua_transport_info ti;
      pjsua_transport_get_info(m_udpTransportId, &ti);
      AccountConfig local;
      local.sipConfig.ipv6Use = PJSUA_IPV6_ENABLED_PREFER_IPV4;
      local.idUri =
          "<sip:" +
          std::string(ti.local_name.host.ptr, ti.local_name.host.slen) + ":" +
          std::to_string(ti.local_name.port) + ">";
      local.videoConfig.autoShowIncoming = true;
      local.videoConfig.autoTransmitOutgoing = true;
      m_localAccount = std::make_shared<MyAccount>();
      m_localAccount->create(local);
    } catch (Error &err) {
      m_localAccount.reset();
      qWarning() << "Could not create local account for IP calls:"
                 << QString::fromStdString(err.info());
    }

    // Re-register and refresh transports when the network comes back.
    // ponytail: only sees reachability transitions, not an address change
    // while staying online; watch QNetworkInterface if that turns out to matter.
    if (QNetworkInformation::loadBackendByFeatures(
            QNetworkInformation::Feature::Reachability)) {
      connect(QNetworkInformation::instance(),
              &QNetworkInformation::reachabilityChanged, this,
              [this](QNetworkInformation::Reachability reachability) {
                if (!m_endpoint ||
                    reachability == QNetworkInformation::Reachability::Disconnected)
                  return;
                try {
                  IpChangeParam prm;
                  m_endpoint->handleIpChange(prm);
                  qInfo() << "Network change handled, re-registering";
                } catch (Error &err) {
                  qWarning() << "Error handling network change:"
                             << QString::fromStdString(err.info());
                }
              });
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
      // Accounts must go before the endpoint
      runOnSipThread([this] {
        m_localAccount.reset();
        m_accounts.clear();
      });
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
      BlfService::instance().unsubscribe(id);
      // An account deletes the buddies attached to it, so release ours first
      runOnSipThread([this, &id] {
        BuddyService::instance().clear();
        m_accounts.remove(id);
      });
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
      cfg.sipConfig.transportId = m_tlsTransportId;
    } else if (transport == "udp") {
      transportParam = ";transport=udp";
      cfg.sipConfig.transportId = config["ipVersion"].toInt() == 1
                                      ? m_udp6TransportId
                                      : m_udpTransportId;
    }

    QString displayName = config["displayName"].toString().remove('"');
    cfg.idUri = "<sip:" + (username + "@" + domain).toStdString() + ">";
    if (!displayName.isEmpty())
      cfg.idUri = "\"" + displayName.toStdString() + "\" " + cfg.idUri;

    QString server = config["server"].toString();
    if (server.isEmpty())
      server = domain;
    cfg.regConfig.registrarUri = "sip:" + server.toStdString() + transportParam;
    if (config["registerRefresh"].toInt() > 0)
      cfg.regConfig.timeoutSec = config["registerRefresh"].toInt();
    cfg.presConfig.publishEnabled = config["publishPresence"].toBool();

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
    // Media falls back in ICE order: direct (host candidates), then the
    // STUN-mapped address, then the TURN relay. A relay is an ICE candidate,
    // so configuring TURN turns ICE on.
    const QString turn = config["turnServer"].toString().trimmed();
    cfg.natConfig.iceEnabled =
        (config["ice"].toBool() || !turn.isEmpty()) ? PJ_TRUE : PJ_FALSE;

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
    if (ipVersion == 0) { // IPv4 only
      cfg.sipConfig.ipv6Use = PJSUA_IPV6_DISABLED;
      cfg.mediaConfig.ipv6Use = PJSUA_IPV6_DISABLED;
    } else if (ipVersion == 1) { // IPv6 only
      // ponytail: signalling is IPv6 over UDP only; TCP and TLS accounts still
      // connect over IPv4. Add TCP6/TLS6 transports if that is ever needed.
      cfg.sipConfig.ipv6Use = PJSUA_IPV6_ENABLED_USE_IPV6_ONLY;
      cfg.mediaConfig.ipv6Use = PJSUA_IPV6_ENABLED_USE_IPV6_ONLY;
    } else { // Auto: IPv4 signalling, media may use IPv6 if IPv4 is not offered
      cfg.mediaConfig.ipv6Use = PJSUA_IPV6_ENABLED_PREFER_IPV4;
    }

    // Only calls started with video carry a video stream (see answerCall)
    cfg.videoConfig.autoShowIncoming = true;
    cfg.videoConfig.autoTransmitOutgoing = true;

    auto acc = std::make_shared<MyAccount>();
    acc->key = id;
    acc->create(cfg);
    m_accounts[id] = acc;

    qDebug() << "Registered account:" << id;
    BlfService::instance().subscribe(id, config["blfUri"].toString());

  } catch (Error &err) {
    qCritical() << "Error registering account:"
                << QString::fromStdString(err.info());
    emit accountStateChanged(id, false, 500,
                             QString::fromStdString(err.info()));
  }
}

void SipService::unregisterAccount(const QString &id) {
  if (m_accounts.contains(id)) {
    BlfService::instance().unsubscribe(id);
    runOnSipThread([this, &id] {
      BuddyService::instance().clear();
      m_accounts.remove(id); // ~Account unregisters and deletes the account
    });
    qDebug() << "Unregistered account:" << id;
    emit accountStateChanged(id, false, 0, "Unregistered");
  }
}

void MyAccount::onRegState(pj::OnRegStateParam &prm) {
  if (key.isEmpty()) // local account
    return;
  AccountInfo ai = getInfo();
  qDebug() << (ai.regIsActive ? "*** Register:" : "*** Unregister:")
           << " code=" << prm.code;

  emit SipService::instance().accountStateChanged(
      key, ai.regIsActive, prm.code,
      QString::fromStdString(prm.reason));
}
void MyAccount::onIncomingCall(pj::OnIncomingCallParam &prm) {
  // PJSUA2 rejects the call with 500 unless a Call object takes it, so a call
  // refused by Do Not Disturb still gets one; it ends up in history as Missed.
  auto call = std::make_shared<MyCall>(*this, prm.callId);
  const int callId = prm.callId;
  const bool rejected = SipService::instance().doNotDisturb();
  QString remoteUri;
  try {
    remoteUri = QString::fromStdString(call->getInfo().remoteUri);
  } catch (Error &) {
  }

  // PJSIP worker thread: hand the call over to the GUI thread. Queued before
  // answering so it runs ahead of any state change the answer triggers.
  QMetaObject::invokeMethod(
      &CallService::instance(),
      [call, callId, remoteUri, rejected] {
        CallService::instance().addCall(call);
        if (rejected)
          return;
        emit CallService::instance().incomingCall(callId, remoteUri);
        if (SipService::instance().autoAnswer())
          CallService::instance().answerCall(callId);
      },
      Qt::QueuedConnection);

  try {
    CallOpParam response;
    response.statusCode = rejected ? PJSIP_SC_BUSY_HERE : PJSIP_SC_RINGING;
    if (rejected)
      call->hangup(response);
    else
      call->answer(response);
  } catch (Error &err) {
    qWarning() << "Error responding to incoming call:"
               << QString::fromStdString(err.info());
  }
}

void MyAccount::onInstantMessage(pj::OnInstantMessageParam &prm) {
  const QString from = bareSipUri(QString::fromStdString(prm.fromUri));
  const QString content = QString::fromStdString(prm.msgBody);
  StorageService::instance().addMessage(from, "Incoming", content);
  emit SipService::instance().messageReceived(from, content);
}

// Delivery report for a message we sent
void MyAccount::onInstantMessageStatus(pj::OnInstantMessageStatusParam &prm) {
  if (prm.code < 300)
    return;
  qWarning() << "Message not delivered:" << prm.code
             << QString::fromStdString(prm.reason);
  StorageService::instance().markMessageFailed(
      QString::fromStdString(prm.toUri), QString::fromStdString(prm.msgBody));
}

void MyAccount::onTypingIndication(pj::OnTypingIndicationParam &prm) {
  QString fromUri = QString::fromStdString(prm.fromUri);
  qDebug() << "Typing indication from" << fromUri << ":"
           << (prm.isTyping ? "Typing..." : "Stopped");

  emit SipService::instance().typingIndicationReceived(fromUri, prm.isTyping);
}

// Accepts "1001", "1001@host" or a full sip: URI; a bare user gets the
// account's domain (accountId is "user@domain").
static std::string toSipUri(const QString &destination,
                            const QString &accountId) {
  QString dest = bareSipUri(destination);
  if (!dest.contains('@'))
    dest += accountId.mid(accountId.indexOf('@'));
  return ("sip:" + dest).toStdString();
}

bool SipService::sendMessage(const QString &accountId,
                             const QString &destination,
                             const QString &content) {
  auto acc = getAccount(accountId);
  pj_status_t status = PJ_EINVAL;
  if (acc) {
    std::string destStr = toSipUri(destination, accountId);
    std::string contentStr = content.toStdString();
    pj_str_t pj_dest = pj_str((char *)destStr.c_str());
    pj_str_t pj_content = pj_str((char *)contentStr.c_str());
    status = pjsua_im_send(acc->getId(), &pj_dest, nullptr, &pj_content,
                           nullptr, nullptr);
  }
  if (status != PJ_SUCCESS)
    qWarning() << "Could not send message to" << destination << "status"
               << status;

  // A later delivery failure flips "Outgoing" to "Failed"
  StorageService::instance().addMessage(
      destination, status == PJ_SUCCESS ? "Outgoing" : "Failed", content);
  return status == PJ_SUCCESS;
}

void SipService::sendTypingIndication(const QString &accountId,
                                      const QString &destination,
                                      bool isTyping) {
  auto acc = getAccount(accountId);
  if (!acc)
    return;

  std::string destStr = toSipUri(destination, accountId);
  pj_str_t pj_dest = pj_str((char *)destStr.c_str());
  pj_status_t status =
      pjsua_im_typing(acc->getId(), &pj_dest, isTyping, nullptr);
  if (status != PJ_SUCCESS)
    qWarning() << "Error sending typing indication:" << status;
}
