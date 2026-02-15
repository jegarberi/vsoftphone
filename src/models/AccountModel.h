#ifndef ACCOUNTMODEL_H
#define ACCOUNTMODEL_H

#include <QAbstractListModel>
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
  bool active;
};

class AccountModel : public QAbstractListModel {
  Q_OBJECT
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
    ActiveRole
  };

  explicit AccountModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  Q_INVOKABLE void addAccount(const QVariantMap &accountData);
  Q_INVOKABLE void updateAccount(int index, const QVariantMap &accountData);
  Q_INVOKABLE QVariantMap getAccount(int index) const;
  Q_INVOKABLE void removeAccount(int index);
  Q_INVOKABLE void toggleActive(int index);

private:
  QList<Account> m_accounts;
};

#endif // ACCOUNTMODEL_H
