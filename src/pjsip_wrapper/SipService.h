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

public:
  static SipService &instance();

  void init();
  void shutdown();

  void registerAccount(const QString &id, const QVariantMap &config);
  void unregisterAccount(const QString &id);

  void sendMessage(const QString &accountId, const QString &destination,
                   const QString &content);
  Q_INVOKABLE void sendTypingIndication(const QString &accountId,
                            const QString &destination, bool isTyping);

  std::shared_ptr<MyAccount> getAccount(const QString &id) {
    return m_accounts.value(id);
  }
  pj::Endpoint *endpoint() { return m_endpoint; }

  bool doNotDisturb() const { return m_doNotDisturb; }
  void setDoNotDisturb(bool enabled);
  bool autoAnswer() const { return m_autoAnswer; }
  void setAutoAnswer(bool enabled);

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
  void callHandlingChanged();

private:
  explicit SipService(QObject *parent = nullptr);
  ~SipService();

  SipService(const SipService &) = delete;
  SipService &operator=(const SipService &) = delete;

  pj::Endpoint *m_endpoint;
  // Read from the PJSIP thread in onIncomingCall
  std::atomic<bool> m_doNotDisturb{false};
  std::atomic<bool> m_autoAnswer{false};
  pj::TransportId m_udpTransportId = PJSUA_INVALID_ID;
  pj::TransportId m_tcpTransportId = PJSUA_INVALID_ID;
  pj::TransportId m_tlsTransportId = PJSUA_INVALID_ID;
  QMap<QString, std::shared_ptr<MyAccount>> m_accounts;
};

#endif // SIPSERVICE_H
