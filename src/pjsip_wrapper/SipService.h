#ifndef SIPSERVICE_H
#define SIPSERVICE_H

#include <QMap>
#include <QObject>
#include <QVariantMap>
#include <atomic>
#include <memory>
#include <pjsua2.hpp>

// Forward declarations
namespace pj {
class Endpoint;
}

class MyAccount : public pj::Account {
public:
  MyAccount() {}
  ~MyAccount() {}

  QString key; // "user@domain", the id used by SipService and the models

  virtual void onRegState(pj::OnRegStateParam &prm) override;
  virtual void onIncomingCall(pj::OnIncomingCallParam &prm) override;
  virtual void onInstantMessage(pj::OnInstantMessageParam &prm) override;
  virtual void
  onInstantMessageStatus(pj::OnInstantMessageStatusParam &prm) override;
  virtual void onTypingIndication(pj::OnTypingIndicationParam &prm) override;
};

class SipService : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool doNotDisturb READ doNotDisturb WRITE setDoNotDisturb NOTIFY
                 callHandlingChanged)
  Q_PROPERTY(bool autoAnswer READ autoAnswer WRITE setAutoAnswer NOTIFY
                 callHandlingChanged)
  // Checked when the TLS transport is created, so a change needs a restart
  Q_PROPERTY(bool tlsVerify READ tlsVerify WRITE setTlsVerify NOTIFY
                 callHandlingChanged)

  // Network settings shared by all accounts. STUN and TURN are the defaults
  // for accounts that name no server of their own. Changes reach the accounts
  // through applyNetworkSettings().
  Q_PROPERTY(bool stunEnabled READ stunEnabled WRITE setStunEnabled NOTIFY
                 networkSettingsChanged)
  Q_PROPERTY(QString stunServer READ stunServer WRITE setStunServer NOTIFY
                 networkSettingsChanged)
  Q_PROPERTY(bool turnEnabled READ turnEnabled WRITE setTurnEnabled NOTIFY
                 networkSettingsChanged)
  Q_PROPERTY(QString turnServer READ turnServer WRITE setTurnServer NOTIFY
                 networkSettingsChanged)
  Q_PROPERTY(QString turnUser READ turnUser WRITE setTurnUser NOTIFY
                 networkSettingsChanged)
  Q_PROPERTY(QString turnPassword READ turnPassword WRITE setTurnPassword
                 NOTIFY networkSettingsChanged)
  // Ask servers to reply to the port a request came from (Via ;rport)
  Q_PROPERTY(bool rport READ rport WRITE setRport NOTIFY networkSettingsChanged)
  // Advertise in SDP the address the server saw us at, learned through rport
  Q_PROPERTY(bool mediaRport READ mediaRport WRITE setMediaRport NOTIFY
                 networkSettingsChanged)

public:
  static SipService &instance();

  void init();
  void shutdown();

  void registerAccount(const QString &id, const QVariantMap &config);
  void unregisterAccount(const QString &id);

  // False when the message could not even be handed to the SIP stack
  bool sendMessage(const QString &accountId, const QString &destination,
                   const QString &content);
  Q_INVOKABLE void sendTypingIndication(const QString &accountId,
                            const QString &destination, bool isTyping);

  std::shared_ptr<MyAccount> getAccount(const QString &id) {
    return m_accounts.value(id);
  }
  // Unregistered account for direct IP calls; exists even with no SIP account
  std::shared_ptr<MyAccount> localAccount() { return m_localAccount; }
  pj::Endpoint *endpoint() { return m_endpoint; }

  bool doNotDisturb() const { return m_doNotDisturb; }
  void setDoNotDisturb(bool enabled);
  bool autoAnswer() const { return m_autoAnswer; }
  void setAutoAnswer(bool enabled);
  bool tlsVerify() const;
  void setTlsVerify(bool enabled);

  bool stunEnabled() const { return net("stunEnabled", true).toBool(); }
  void setStunEnabled(bool v) { setNet("stunEnabled", v); }
  QString stunServer() const {
    return net("stunServer", "stun.l.google.com:19302").toString();
  }
  void setStunServer(const QString &v) { setNet("stunServer", v.trimmed()); }
  bool turnEnabled() const { return net("turnEnabled", false).toBool(); }
  void setTurnEnabled(bool v) { setNet("turnEnabled", v); }
  QString turnServer() const { return net("turnServer", "").toString(); }
  void setTurnServer(const QString &v) { setNet("turnServer", v.trimmed()); }
  QString turnUser() const { return net("turnUser", "").toString(); }
  void setTurnUser(const QString &v) { setNet("turnUser", v); }
  QString turnPassword() const { return m_turnPassword; }
  void setTurnPassword(const QString &password);
  bool rport() const { return net("rport", true).toBool(); }
  void setRport(bool enabled);
  bool mediaRport() const { return net("mediaRport", false).toBool(); }
  void setMediaRport(bool v) { setNet("mediaRport", v); }
  // Re-registers the active accounts if a network setting changed
  Q_INVOKABLE void applyNetworkSettings();

  QList<std::shared_ptr<MyAccount>> getAccounts() const {
    return m_accounts.values();
  }

  void notifyAccountStateChanged(QString id, bool active, int statusCode,
                                 QString statusText) {
    emit accountStateChanged(id, active, statusCode, statusText);
  }

signals:
  void initStatusChanged(bool success, QString message);
  void accountStateChanged(QString id, bool active, int statusCode,
                           QString statusText);
  void typingIndicationReceived(QString fromUri, bool isTyping);
  void messageReceived(QString from, QString text);
  void callHandlingChanged();
  void networkSettingsChanged();
  void networkSettingsApplied();

private:
  explicit SipService(QObject *parent = nullptr);
  ~SipService();

  SipService(const SipService &) = delete;
  SipService &operator=(const SipService &) = delete;

  QVariant net(const char *key, const QVariant &fallback) const;
  void setNet(const char *key, const QVariant &value);
  QString m_turnPassword;
  bool m_networkDirty = false;

  pj::Endpoint *m_endpoint;
  // Read from the PJSIP thread in onIncomingCall
  std::atomic<bool> m_doNotDisturb{false};
  std::atomic<bool> m_autoAnswer{false};
  pj::TransportId m_udpTransportId = PJSUA_INVALID_ID;
  pj::TransportId m_tcpTransportId = PJSUA_INVALID_ID;
  pj::TransportId m_tlsTransportId = PJSUA_INVALID_ID;
  pj::TransportId m_udp6TransportId = PJSUA_INVALID_ID;
  QMap<QString, std::shared_ptr<MyAccount>> m_accounts;
  std::shared_ptr<MyAccount> m_localAccount;
};

#endif // SIPSERVICE_H
