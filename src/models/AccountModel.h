#ifndef ACCOUNTMODEL_H
#define ACCOUNTMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QString>
#include <QVariantMap>

#include "../pjsip_wrapper/StorageService.h"

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
    BlfUriRole,
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
  // Makes the next active account the one used for calls and messages
  Q_INVOKABLE void selectNextAccount();

signals:
  void defaultAccountIdChanged();

private:
  QList<AccountRecord> m_accounts;
  QString m_selectedId; // preferred account; falls back to the first active
  QHash<QString, QString> m_status; // "user@domain" -> registration status
};

#endif // ACCOUNTMODEL_H
