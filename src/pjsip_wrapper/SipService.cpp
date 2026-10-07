#include "SipService.h"
#include "BlfService.h"
#include "BuddyService.h"
#include "CallService.h"
#include "Keyring.h"
#include "PresenceService.h"
#include "HardwareService.h"
#include "StorageService.h"
#include "SipUri.h"
#include <QDebug>
#include <QFile>
#include <QNetworkInformation>
#include <QSettings>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QStandardPaths>
#include <QTimer>
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
  m_acceptIpCalls = settings.value("calls/acceptIpCalls", true).toBool();

  // Emitted from the PJSIP thread; `this` as context queues it to ours
  connect(this, &SipService::accountStateChanged, this,
          [this](QString id, bool active, int code, QString) {
            onRegistrationResult(id, active, code);
          });

  // The global TURN password: system store when there is one, settings file
  // otherwise, and back to the settings file if storing it fails
  connect(&SecretStore::instance(), &SecretStore::stored, this,
          [](const QString &key) {
            if (key == SecretStore::TurnPasswordKey)
              QSettings().remove("network/turnPassword");
          });
  connect(&SecretStore::instance(), &SecretStore::storeFailed, this,
          [this](const QString &key) {
            if (key == SecretStore::TurnPasswordKey)
              QSettings().setValue("network/turnPassword", turnPassword());
          });
}

// The proxies an account may use, in order of preference
static QStringList proxiesOf(const QVariantMap &config) {
  QStringList proxies;
  for (const char *key : {"proxy", "proxy2"}) {
    const QString proxy = config[key].toString().trimmed();
    if (!proxy.isEmpty())
      proxies.append(proxy);
  }
  return proxies;
}

static QString transportParamOf(const QVariantMap &config) {
  const QString transport = config["transport"].toString().toLower();
  return (transport == "tcp" || transport == "tls" || transport == "udp")
             ? ";transport=" + transport
             : QString();
}

// A registration that failed because nothing answered, or the proxy said it
// cannot serve, is a reason to try the other proxy. A refused login is not.
void SipService::onRegistrationResult(const QString &id, bool active,
                                      int code) {
  if (active || !m_configs.contains(id) || m_proxyChoice.value(id) != 0)
    return;
  if (code != 408 && code != 502 && code != 503 && code != 504)
    return;
  const QVariantMap config = m_configs.value(id);
  if (proxiesOf(config).size() < 2)
    return;
  qWarning() << id << "cannot register through its first proxy (" << code
             << "), switching to the second";
  m_proxyChoice[id] = 1;
  applyAccount(id, config);
  emit proxyChanged(id);
}

// Accounts that fell back to their second proxy go back to the first if it
// answers now; otherwise they stay where they are.
void SipService::recheckProxies() {
  const QStringList ids = m_proxyChoice.keys();
  for (const QString &id : ids) {
    if (m_proxyChoice.value(id) != 1 || !m_configs.contains(id))
      continue;
    const QVariantMap config = m_configs.value(id);
    const QString first = proxiesOf(config).value(0) + transportParamOf(config);
    probeProxy(id, first, [this, id](bool reachable) {
      if (!reachable || m_proxyChoice.value(id) != 1 || !m_configs.contains(id))
        return;
      qInfo() << id << "first proxy answers again, switching back to it";
      m_proxyChoice[id] = 0;
      applyAccount(id, m_configs.value(id));
      emit proxyChanged(id);
    });
  }
}

void SipService::onNetworkChanged() {
  if (!m_endpoint)
    return;
  try {
    IpChangeParam prm;
    m_endpoint->handleIpChange(prm);
    qInfo() << "Network change handled, re-registering";
  } catch (Error &err) {
    qWarning() << "Error handling network change:"
               << QString::fromStdString(err.info());
  }
  recheckProxies();
}

// Sends a SIP OPTIONS to `target` ("host:port;transport=..") and reports
// whether anything answered. Any SIP response counts, even an error: it shows
// the proxy is there.
void SipService::probeProxy(const QString &id, const QString &target,
                            std::function<void(bool)> done) {
  // Decided once, on this thread, by whichever comes first: the response or
  // our own time limit (PJSIP itself only gives up after 32 seconds)
  auto decided = std::make_shared<bool>(false);
  auto decide = [decided, done](bool reachable) {
    if (*decided)
      return;
    *decided = true;
    done(reachable);
  };

  const std::string to = ("sip:" + target).toStdString();
  const std::string from = ("<sip:" + id + ">").toStdString();
  pj_str_t pjTo = pj_str(const_cast<char *>(to.c_str()));
  pj_str_t pjFrom = pj_str(const_cast<char *>(from.c_str()));
  pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
  pjsip_tx_data *tdata = nullptr;
  pj_status_t status = pjsip_endpt_create_request(
      endpt, pjsip_get_options_method(), &pjTo, &pjFrom, &pjTo, nullptr,
      nullptr, -1, nullptr, &tdata);
  if (status == PJ_SUCCESS) {
    auto *token = new std::function<void(bool)>(decide);
    status = pjsip_endpt_send_request(
        endpt, tdata, -1, token, [](void *token, pjsip_event *event) {
          // PJSIP thread
          auto *report = static_cast<std::function<void(bool)> *>(token);
          const int code = event->body.tsx_state.tsx->status_code;
          const bool reachable = code != 408 && code != 502 && code != 503;
          if (pjsua_get_state() != PJSUA_STATE_RUNNING) {
            delete report; // shutting down
            return;
          }
          QMetaObject::invokeMethod(
              &SipService::instance(),
              [report, reachable] {
                (*report)(reachable);
                delete report;
              },
              Qt::QueuedConnection);
        });
    if (status != PJ_SUCCESS)
      delete token;
  }
  if (status != PJ_SUCCESS) {
    decide(false);
    return;
  }

  auto *limit = new QTimer(this);
  limit->setSingleShot(true);
  connect(limit, &QTimer::timeout, this, [decide, limit] {
    limit->deleteLater();
    decide(false);
  });
  limit->start(5000);
}

QString SipService::turnPassword() const {
  bool found = false;
  const QString secret =
      SecretStore::instance().get(SecretStore::TurnPasswordKey, &found);
  return found ? secret : QSettings().value("network/turnPassword").toString();
}

// Accounts changed while in a call were only modified in place, which leaves
// a few settings behind. Once such an account is idle it is rebuilt properly.
void SipService::applyDeferredAccounts() {
  const QStringList ids = m_deferred.keys();
  for (const QString &id : ids) {
    if (!m_accounts.contains(id)) {
      m_deferred.remove(id);
    } else if (!hasActiveCalls(id)) {
      applyAccount(id, m_deferred.take(id));
    }
  }
}

QVariant SipService::net(const char *key, const QVariant &fallback) const {
  return QSettings().value(QLatin1String("network/") + QLatin1String(key),
                           fallback);
}

// Unchanged values are ignored: applying settings re-registers accounts,
// which should happen only when something they use is different.
void SipService::setNet(const char *key, const QVariant &value,
                        const QVariant &fallback) {
  if (net(key, fallback).toString() == value.toString())
    return;
  QSettings().setValue(QLatin1String("network/") + QLatin1String(key), value);
  m_networkDirty = true;
  emit networkSettingsChanged();
}

void SipService::setAcceptIpCalls(bool enabled) {
  if (m_acceptIpCalls.exchange(enabled) == enabled)
    return;
  QSettings().setValue("calls/acceptIpCalls", enabled);
  emit networkSettingsChanged();
}

bool SipService::hasActiveCalls(const QString &id) const {
  const auto acc = m_accounts.value(id);
  if (!acc)
    return false;
  pjsua_call_id ids[PJSUA_MAX_CALLS];
  unsigned count = PJ_ARRAY_SIZE(ids);
  if (pjsua_enum_calls(ids, &count) != PJ_SUCCESS)
    return false;
  for (unsigned i = 0; i < count; ++i) {
    pjsua_call_info info;
    if (pjsua_call_get_info(ids[i], &info) == PJ_SUCCESS &&
        info.acc_id == acc->getId())
      return true;
  }
  return false;
}

void SipService::setTurnPassword(const QString &password) {
  if (turnPassword() == password)
    return;
  SecretStore::instance().set(SecretStore::TurnPasswordKey, password);
  if (!Keyring::available())
    QSettings().setValue("network/turnPassword", password);
  m_networkDirty = true;
  emit networkSettingsChanged();
}

// A global PJSIP switch, so it takes effect at once for every account
void SipService::setRport(bool enabled) {
  QSettings().setValue("network/rport", enabled);
  if (m_endpoint)
    pjsip_cfg()->endpt.disable_rport = enabled ? PJ_FALSE : PJ_TRUE;
  emit networkSettingsChanged();
}

void SipService::applyNetworkSettings() {
  if (!m_networkDirty)
    return;
  m_networkDirty = false;
  emit networkSettingsApplied();
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
    pjsip_cfg()->endpt.disable_rport = rport() ? PJ_FALSE : PJ_TRUE;

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
#ifdef Q_OS_UNIX
        for (const char *bundle : {"/etc/ssl/certs/ca-certificates.crt",
                                   "/etc/pki/tls/certs/ca-bundle.crt",
                                   "/etc/ssl/cert.pem"}) {
          if (QFile::exists(bundle)) {
            tls_cfg.tlsConfig.CaListFile = bundle;
            break;
          }
        }
#endif
        // No bundle file (Windows, macOS): write out the system trust store,
        // which Qt can read on every platform
        if (tls_cfg.tlsConfig.CaListFile.empty()) {
          QFile bundle(QStandardPaths::writableLocation(
                           QStandardPaths::AppDataLocation) +
                       "/ca-bundle.pem");
          if (bundle.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            const auto certificates =
                QSslConfiguration::systemCaCertificates();
            for (const QSslCertificate &certificate : certificates)
              bundle.write(certificate.toPem());
            bundle.close();
            if (certificates.isEmpty())
              qWarning() << "No system CA certificates found: TLS servers "
                            "cannot be verified, so TLS accounts will fail";
            else
              qInfo() << "TLS verifies against" << certificates.size()
                      << "system CA certificates";
            tls_cfg.tlsConfig.CaListFile =
                QFile::encodeName(bundle.fileName()).toStdString();
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
      // Direct calls must not wait on, or fail with, a STUN server
      local.natConfig.sipStunUse = PJSUA_STUN_USE_DISABLED;
      local.natConfig.mediaStunUse = PJSUA_STUN_USE_DISABLED;
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
                if (reachability !=
                    QNetworkInformation::Reachability::Disconnected)
                  onNetworkChanged();
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

// A new or edited account starts on its first proxy
void SipService::registerAccount(const QString &id, const QVariantMap &config) {
  m_configs[id] = config;
  m_proxyChoice[id] = 0;
  applyAccount(id, config);
}

// Builds the PJSIP account from `config`, on whichever proxy it is using now
void SipService::applyAccount(const QString &id, const QVariantMap &config) {
  if (!m_endpoint)
    return;

  try {
    AccountConfig cfg;
    QString username = config["username"].toString();
    QString domain = config["domain"].toString();
    QString password = config["password"].toString();
    const QStringList proxies = proxiesOf(config);
    const QString proxy =
        proxies.value(qMin(m_proxyChoice.value(id), proxies.size() - 1));

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

    // NAT traversal
    const bool autoNat = config["autoNat"].toBool();
    const bool allowIpRewrite = config["allowIpRewrite"].toBool();
    // Use the address the server saw us at (learned from received/rport) in
    // Contact and Via, and, for media, in SDP
    cfg.natConfig.contactRewriteUse = (autoNat || allowIpRewrite) ? 1 : 0;
    cfg.natConfig.viaRewriteUse = autoNat ? 1 : 0;
    cfg.natConfig.sdpNatRewriteUse =
        (autoNat || allowIpRewrite || mediaRport()) ? 1 : 0;

    // The account's own STUN and TURN servers win over the global ones
    const QString stunDefault = stunEnabled() ? stunServer() : QString();
    QString turn = config["turnServer"].toString().trimmed();
    QString turnUserName = config["turnUser"].toString();
    QString turnPass = config["turnPass"].toString();
    if (turn.isEmpty() && turnEnabled()) {
      turn = turnServer();
      turnUserName = turnUser();
      turnPass = turnPassword();
    }

    // Media falls back in ICE order: direct (host candidates), then the
    // STUN-mapped address, then the TURN relay. A relay is an ICE candidate,
    // so configuring TURN turns ICE on.
    cfg.natConfig.iceEnabled =
        (config["ice"].toBool() || !turn.isEmpty()) ? PJ_TRUE : PJ_FALSE;

    const bool enableStun =
        config["enableStun"].toBool() &&
        !(config["stunServer"].toString().trimmed().isEmpty() &&
          stunDefault.isEmpty());
    cfg.natConfig.sipStunUse =
        enableStun ? PJSUA_STUN_USE_DEFAULT : PJSUA_STUN_USE_DISABLED;
    cfg.natConfig.mediaStunUse =
        enableStun ? PJSUA_STUN_USE_DEFAULT : PJSUA_STUN_USE_DISABLED;

    // PJSIP has one STUN server list: every server an active account uses
    QSet<QString> activeStunServers;
    const QList<AccountRecord> records = StorageService::instance().getAccounts();
    for (const auto &rec : records) {
      if (!rec.active || !rec.enableStun)
        continue;
      const QString server =
          rec.stunServer.isEmpty() ? stunDefault : rec.stunServer;
      if (!server.isEmpty())
        activeStunServers.insert(server);
    }

    std::vector<std::string> stunStdStrs;
    for (const QString &server : activeStunServers)
      stunStdStrs.push_back(server.toStdString());

    // PJSIP asserts on an empty list, so a list is only ever replaced by a
    // non-empty one; accounts without STUN have it disabled above anyway.
    if (!stunStdStrs.empty()) {
      std::vector<pj_str_t> servers;
      for (const std::string &server : stunStdStrs)
        servers.push_back(pj_str(const_cast<char *>(server.c_str())));
      const pj_status_t status =
          pjsua_update_stun_servers(servers.size(), servers.data(), PJ_FALSE);
      if (status != PJ_SUCCESS)
        qWarning() << "Failed to update STUN servers:" << status;
    }

    if (!turn.isEmpty()) {
      cfg.natConfig.turnEnabled = PJ_TRUE;
      cfg.natConfig.turnServer = turn.toStdString();
      cfg.natConfig.turnConnType = PJ_TURN_TP_UDP;
      cfg.natConfig.turnUserName = turnUserName.toStdString();
      cfg.natConfig.turnPassword = turnPass.toStdString();
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
    cfg.videoConfig.defaultCaptureDevice =
        HardwareService::instance().selectedVideoDevice();

    auto existing = m_accounts.value(id);
    if (existing && hasActiveCalls(id)) {
      // Deleting the account would pull its data out from under the call, so
      // it is modified in place.
      // PJSIP's modify skips a few fields (the SDP address rewrite among
      // them), so the full rebuild is redone when the call is over.
      existing->modify(cfg);
      m_deferred[id] = config;
    } else {
      m_deferred.remove(id);
      if (existing) {
        BlfService::instance().unsubscribe(id);
        // An account deletes the buddies attached to it: release ours first
        // `existing` is a reference to the account too: the last one must be
        // dropped on that thread, or the deletion happens here after all
        runOnSipThread([this, &id, &existing] {
          BuddyService::instance().clear();
          m_accounts.remove(id);
          existing.reset();
        });
      }
      auto acc = std::make_shared<MyAccount>();
      acc->key = id;
      acc->create(cfg);
      m_accounts[id] = acc;
    }
    PresenceService::instance().apply();

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
    m_deferred.remove(id);
    m_configs.remove(id);
    m_proxyChoice.remove(id);
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
  // we refuse still gets one.
  auto call = std::make_shared<MyCall>(*this, prm.callId);
  const int callId = prm.callId;
  // The local account has no key: this call was addressed to our IP address
  const bool blocked = key.isEmpty() && !SipService::instance().acceptIpCalls();
  const bool rejected = blocked || SipService::instance().doNotDisturb();
  // Do Not Disturb leaves a Missed entry; a blocked stranger leaves nothing
  call->silent = blocked;
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
        CallService &calls = CallService::instance();
        if (rejected) {
          calls.addCall(call); // only so that it is released when it ends
          return;
        }
        calls.announceIncoming(call, remoteUri);
        // Auto answer must not interrupt a conversation already going on
        if (SipService::instance().autoAnswer() &&
            !calls.hasConnectedCall(callId))
          calls.answerCall(callId);
      },
      Qt::QueuedConnection);

  try {
    CallOpParam response;
    response.statusCode = blocked    ? PJSIP_SC_FORBIDDEN
                          : rejected ? PJSIP_SC_BUSY_HERE
                                     : PJSIP_SC_RINGING;
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
