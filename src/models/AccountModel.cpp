#include "AccountModel.h"
#include "../pjsip_wrapper/SipService.h"
#include "../pjsip_wrapper/StorageService.h"

AccountModel::AccountModel(QObject *parent) : QAbstractListModel(parent) {
  QList<AccountRecord> records = StorageService::instance().getAccounts();
  for (const auto &rec : records) {
    m_accounts.append({QString::number(rec.id),
                       rec.accountName,
                       rec.server,
                       rec.proxy,
                       rec.username,
                       rec.domain,
                       rec.username,
                       rec.password,
                       rec.displayName,
                       rec.transport,
                       rec.registerRefresh,
                       rec.publishPresence,
                       rec.iceEnabled,
                       rec.stunServer,
                       rec.turnServer,
                       rec.turnUser,
                       rec.turnPass,
                       rec.srtpMode,
                       true,
                       rec.active});
  }
}

int AccountModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_accounts.count();
}

QVariant AccountModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid())
    return QVariant();

  const Account &account = m_accounts[index.row()];

  switch (role) {
  case IdRole:
    return account.id;
  case AccountNameRole:
    return account.accountName;
  case ServerRole:
    return account.server;
  case ProxyRole:
    return account.proxy;
  case UsernameRole:
    return account.username;
  case DomainRole:
    return account.domain;
  case LoginRole:
    return account.login;
  case PasswordRole:
    return account.password;
  case DisplayNameRole:
    return account.displayName;
  case TransportRole:
    return account.transport;
  case RegisterRefreshRole:
    return account.registerRefresh;
  case PublishPresenceRole:
    return account.publishPresence;
  case IceRole:
    return account.ice;
  case StunServerRole:
    return account.stunServer;
  case TurnServerRole:
    return account.turnServer;
  case TurnUserRole:
    return account.turnUser;
  case TurnPassRole:
    return account.turnPass;
  case SrtpModeRole:
    return account.srtpMode;
  case AllowIpRewriteRole:
    return account.allowIpRewrite;
  case ActiveRole:
    return account.active;
  default:
    return QVariant();
  }
}

QHash<int, QByteArray> AccountModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[IdRole] = "id";
  roles[AccountNameRole] = "accountName";
  roles[ServerRole] = "server";
  roles[ProxyRole] = "proxy";
  roles[UsernameRole] = "username";
  roles[DomainRole] = "domain";
  roles[LoginRole] = "login";
  roles[PasswordRole] = "password";
  roles[DisplayNameRole] = "displayName";
  roles[TransportRole] = "transport";
  roles[RegisterRefreshRole] = "registerRefresh";
  roles[PublishPresenceRole] = "publishPresence";
  roles[IceRole] = "ice";
  roles[StunServerRole] = "stunServer";
  roles[TurnServerRole] = "turnServer";
  roles[TurnUserRole] = "turnUser";
  roles[TurnPassRole] = "turnPass";
  roles[SrtpModeRole] = "srtpMode";
  roles[AllowIpRewriteRole] = "allowIpRewrite";
  roles[ActiveRole] = "active";
  return roles;
}

void AccountModel::addAccount(const QVariantMap &data) {
  AccountRecord rec;
  rec.accountName = data["accountName"].toString();
  rec.server = data["server"].toString();
  rec.proxy = data["proxy"].toString();
  rec.username = data["username"].toString();
  rec.domain = data["domain"].toString();
  rec.password = data["password"].toString();
  rec.displayName = data["displayName"].toString();
  rec.transport = data["transport"].toString();
  rec.registerRefresh = data["registerRefresh"].toInt();
  rec.publishPresence = data["publishPresence"].toBool();
  rec.iceEnabled = data["ice"].toBool();
  rec.stunServer = data["stunServer"].toString();
  rec.turnServer = data["turnServer"].toString();
  rec.turnUser = data["turnUser"].toString();
  rec.turnPass = data["turnPass"].toString();
  rec.srtpMode = data["srtpMode"].toInt();
  rec.active = true;

  StorageService::instance().addAccount(rec);

  // Refresh model
  beginResetModel();
  m_accounts.clear();
  QList<AccountRecord> records = StorageService::instance().getAccounts();
  for (const auto &r : records) {
    m_accounts.append({QString::number(r.id),
                       r.accountName,
                       r.server,
                       r.proxy,
                       r.username,
                       r.domain,
                       r.username,
                       r.password,
                       r.displayName,
                       r.transport,
                       r.registerRefresh,
                       r.publishPresence,
                       r.iceEnabled,
                       r.stunServer,
                       r.turnServer,
                       r.turnUser,
                       r.turnPass,
                       r.srtpMode,
                       true,
                       r.active});
  }
  endResetModel();
}

void AccountModel::updateAccount(int index, const QVariantMap &data) {
  if (index < 0 || index >= m_accounts.count())
    return;

  Account &account = m_accounts[index];
  QString oldId = account.username + "@" + account.domain;
  bool wasActive = account.active;

  // Update struct
  account.accountName = data["accountName"].toString();
  account.server = data["server"].toString();
  account.proxy = data["proxy"].toString();
  account.username = data["username"].toString();
  account.domain = data["domain"].toString();
  account.password = data["password"].toString();
  account.displayName = data["displayName"].toString();
  account.transport = data["transport"].toString();
  account.registerRefresh = data["registerRefresh"].toInt();
  account.publishPresence = data["publishPresence"].toBool();
  account.ice = data["ice"].toBool();
  account.stunServer = data["stunServer"].toString();
  account.turnServer = data["turnServer"].toString();
  account.turnUser = data["turnUser"].toString();
  account.turnPass = data["turnPass"].toString();
  account.srtpMode = data["srtpMode"].toInt();

  // Update DB
  AccountRecord rec;
  rec.id = account.id.toInt();
  rec.accountName = account.accountName;
  rec.server = account.server;
  rec.proxy = account.proxy;
  rec.username = account.username;
  rec.domain = account.domain;
  rec.password = account.password;
  rec.displayName = account.displayName;
  rec.transport = account.transport;
  rec.registerRefresh = account.registerRefresh;
  rec.publishPresence = account.publishPresence;
  rec.iceEnabled = account.ice;
  rec.stunServer = account.stunServer;
  rec.turnServer = account.turnServer;
  rec.turnUser = account.turnUser;
  rec.turnPass = account.turnPass;
  rec.srtpMode = account.srtpMode;
  rec.active = account.active;
  StorageService::instance().updateAccount(rec);

  QModelIndex modelIndex = createIndex(index, 0);
  emit dataChanged(modelIndex, modelIndex);

  if (wasActive) {
    QString newId = account.username + "@" + account.domain;
    if (oldId != newId) {
      SipService::instance().unregisterAccount(oldId);
    }

    QVariantMap config = data; // Pass all data including new fields
    SipService::instance().registerAccount(newId, config);
  }
}

void AccountModel::removeAccount(int index) {
  if (index < 0 || index >= m_accounts.count())
    return;

  int id = m_accounts[index].id.toInt();
  StorageService::instance().removeAccount(id);

  beginRemoveRows(QModelIndex(), index, index);
  m_accounts.removeAt(index);
  endRemoveRows();
}

void AccountModel::toggleActive(int index) {
  if (index < 0 || index >= m_accounts.count())
    return;

  m_accounts[index].active = !m_accounts[index].active;
  Account &acc = m_accounts[index];

  // Update DB
  AccountRecord rec;
  rec.id = acc.id.toInt();
  rec.accountName = acc.accountName;
  rec.server = acc.server;
  rec.proxy = acc.proxy;
  rec.username = acc.username;
  rec.domain = acc.domain;
  rec.password = acc.password;
  rec.displayName = acc.displayName;
  rec.transport = acc.transport;
  rec.registerRefresh = acc.registerRefresh;
  rec.publishPresence = acc.publishPresence;
  rec.iceEnabled = acc.ice;
  rec.stunServer = acc.stunServer;
  rec.turnServer = acc.turnServer;
  rec.turnUser = acc.turnUser;
  rec.turnPass = acc.turnPass;
  rec.srtpMode = acc.srtpMode;
  rec.active = acc.active;
  StorageService::instance().updateAccount(rec);

  QModelIndex modelIndex = createIndex(index, 0);
  emit dataChanged(modelIndex, modelIndex, {ActiveRole});

  QString id = acc.username + "@" + acc.domain;
  if (acc.active) {
    SipService::instance().registerAccount(id, getAccount(index));
  } else {
    SipService::instance().unregisterAccount(id);
  }
}

QVariantMap AccountModel::getAccount(int index) const {
  if (index < 0 || index >= m_accounts.count())
    return QVariantMap();

  const Account &account = m_accounts[index];
  QVariantMap data;
  data["accountName"] = account.accountName;
  data["server"] = account.server;
  data["proxy"] = account.proxy;
  data["username"] = account.username;
  data["domain"] = account.domain;
  data["login"] = account.login;
  data["password"] = account.password;
  data["displayName"] = account.displayName;
  data["transport"] = account.transport;
  data["registerRefresh"] = account.registerRefresh;
  data["publishPresence"] = account.publishPresence;
  data["ice"] = account.ice;
  data["stunServer"] = account.stunServer;
  data["turnServer"] = account.turnServer;
  data["turnUser"] = account.turnUser;
  data["turnPass"] = account.turnPass;
  data["srtpMode"] = account.srtpMode;
  data["allowIpRewrite"] = account.allowIpRewrite;
  return data;
}
