#ifndef STORAGESERVICE_H
#define STORAGESERVICE_H

#include <QDateTime>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
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

signals:
  void callHistoryChanged();
  void messagesChanged();

private:
  explicit StorageService(QObject *parent = nullptr);
  ~StorageService();

  QSqlDatabase database() const;
  QString m_dbPath;
  bool createTables();
};

#endif // STORAGESERVICE_H
