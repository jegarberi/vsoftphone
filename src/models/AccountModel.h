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
  // Registration status of that account, for the window's status indicator
  Q_PROPERTY(QString defaultAccountStatus READ defaultAccountStatus NOTIFY
                 defaultAccountStatusChanged)
  // False while passwords are still being read from the system store.
  // Accounts are listed but not registered, and must not be edited yet.
  Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)

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
  bool ready() const { return m_ready; }
  QString defaultAccountStatus() const {
    return m_status.value(defaultAccountId());
  }

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  Q_INVOKABLE void addAccount(const QVariantMap &accountData);
  Q_INVOKABLE void updateAccount(int index, const QVariantMap &accountData);
  Q_INVOKABLE QVariantMap getAccount(int index) const;
  Q_INVOKABLE void removeAccount(int index);
  Q_INVOKABLE void toggleActive(int index);
  // True when another row (not `index`) already is username@domain
  Q_INVOKABLE bool isDuplicate(int index, const QString &username,
                               const QString &domain) const;
  // Makes the next active account the one used for calls and messages
  Q_INVOKABLE void selectNextAccount();

signals:
  void defaultAccountIdChanged();
  void readyChanged();
  void defaultAccountStatusChanged();
  // The requested change would delete an account that is in a call
  void accountBusy(QString message);

private:
  bool refuseIfBusy(const AccountRecord &rec);
  void start();
  bool m_ready = false;

  QList<AccountRecord> m_accounts;
  QString m_selectedId; // preferred account; falls back to the first active
  QHash<QString, QString> m_status; // "user@domain" -> registration status
};

#endif // ACCOUNTMODEL_H
