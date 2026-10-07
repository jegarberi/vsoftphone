#ifndef ACCOUNTMODEL_H
#define ACCOUNTMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QString>
#include <QVariantMap>

struct Account {
  QString id;
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
  bool ice;
  QString stunServer;
  QString turnServer;
  QString turnUser;
  QString turnPass;
  int srtpMode; // 0:Disabled, 1:Optional, 2:Mandatory
  bool allowIpRewrite;
  bool autoNat;

  bool enableStun;
  int ipVersion;
  bool rport;
  bool active;
};

class AccountModel : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(QString defaultAccountId READ defaultAccountId NOTIFY
                 defaultAccountIdChanged)

public:
  enum AccountRoles {
    IdRole = Qt::UserRole + 1,
    AccountNameRole,
    ServerRole,
    ProxyRole,
    UsernameRole,
    DomainRole,
    LoginRole,
    PasswordRole,
    DisplayNameRole,
    TransportRole,
    RegisterRefreshRole,
    PublishPresenceRole,
    IceRole,
    StunServerRole,
    TurnServerRole,
    TurnUserRole,
    TurnPassRole,
    SrtpModeRole,
    AllowIpRewriteRole,
    AutoNatRole,
    EnableStunRole,
    IpVersionRole,
    RportRole,
    ActiveRole,
    StatusRole
  };

  static AccountModel &instance();
  explicit AccountModel(QObject *parent = nullptr);

  QString defaultAccountId() const;

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  Q_INVOKABLE void addAccount(const QVariantMap &accountData);
  Q_INVOKABLE void updateAccount(int index, const QVariantMap &accountData);
  Q_INVOKABLE QVariantMap getAccount(int index) const;
  Q_INVOKABLE void removeAccount(int index);
  Q_INVOKABLE void toggleActive(int index);

signals:
  void defaultAccountIdChanged();

private:
  QList<Account> m_accounts;
  QHash<QString, QString> m_status; // "user@domain" -> registration status
};

#endif // ACCOUNTMODEL_H
