#ifndef STORAGESERVICE_H
#define STORAGESERVICE_H

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariantList>

struct CallRecord {
  int id;
  QString remoteParty;
  QString direction;
  QDateTime timestamp;
  int duration;
};

struct AccountRecord {
  int id;
  QString accountName;
  QString server;
  QString proxy;
  QString username;
  QString domain;
  QString login;
  QString password;
  QString displayName;
  QString transport; // UDP, TCP, TLS
  int registerRefresh;
  bool publishPresence;
  bool iceEnabled;
  QString stunServer;
  QString turnServer;
  QString turnUser;
  QString turnPass;
  int srtpMode; // 0:Disabled, 1:Optional, 2:Mandatory
  bool allowIpRewrite;
  bool autoNat;
  bool enableStun;
  int ipVersion; // 0:IPv4, 1:IPv6, 2:Auto
  bool rport;
  bool active;
  QString blfUri; // BroadWorks busy lamp field list URI
  QString proxy2; // outbound proxy used when the first one fails
};

struct ContactRecord {
  int id;
  QString name;
  QString number;
  QString group;
};

struct MessageRecord {
  int id;
  QString remoteParty;
  QString direction;
  QString content;
  QDateTime timestamp;
};

class StorageService : public QObject {
  Q_OBJECT
public:
  static StorageService &instance();

  bool init();

  // Call History
  void addCall(const QString &remoteParty, const QString &direction,
               int duration);
  QList<CallRecord> getCallHistory();
  void clearHistory();

  // Accounts
  void addAccount(const AccountRecord &acc);
  void updateAccount(const AccountRecord &acc);
  void removeAccount(int id);
  QList<AccountRecord> getAccounts();

  // Contacts
  void addContact(const QString &name, const QString &number,
                  const QString &group);
  void removeContact(int id);
  QList<ContactRecord> getContacts();

  // Messages
  void addMessage(const QString &remoteParty, const QString &direction,
                  const QString &content);
  QList<MessageRecord> getMessages(const QString &remoteParty);
  // Marks our latest matching outgoing message as not delivered
  void markMessageFailed(const QString &toUri, const QString &content);
  // Everyone we have exchanged messages with, most recent first
  Q_INVOKABLE QStringList conversations();

signals:
  void callHistoryChanged();
  void messagesChanged();

private:
  explicit StorageService(QObject *parent = nullptr);
  ~StorageService();

  // Passwords live in the system password store when there is one, leaving
  // the database column empty; otherwise they stay in the database. All of
  // it goes through SecretStore, which never blocks this thread.
  QString secretKey(int accountId, const char *field) const;
  QString secretOr(int accountId, const char *field,
                   const QString &dbValue) const;
  QString keepSecret(int accountId, const char *field, const QString &value);
  void writeSecretColumn(const QString &key, const QString &value);
  void loadSecrets();

  QSqlDatabase database() const;
  QString m_dbPath;
  bool createTables();
};

#endif // STORAGESERVICE_H
