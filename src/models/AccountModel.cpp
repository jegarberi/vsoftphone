#include "AccountModel.h"
#include "../pjsip_wrapper/SipService.h"

// The id SipService and the other models know an account by
static QString keyOf(const AccountRecord &a) {
  return a.username + "@" + a.domain;
}

// The map exchanged with the account dialog and SipService::registerAccount
static QVariantMap toMap(const AccountRecord &a) {
  return {{"accountName", a.accountName},
          {"server", a.server},
          {"proxy", a.proxy},
          {"username", a.username},
          {"domain", a.domain},
          {"login", a.login},
          {"password", a.password},
          {"displayName", a.displayName},
          {"transport", a.transport},
          {"registerRefresh", a.registerRefresh},
          {"publishPresence", a.publishPresence},
          {"ice", a.iceEnabled},
          {"stunServer", a.stunServer},
          {"turnServer", a.turnServer},
          {"turnUser", a.turnUser},
          {"turnPass", a.turnPass},
          {"srtpMode", a.srtpMode},
          {"allowIpRewrite", a.allowIpRewrite},
          {"autoNat", a.autoNat},
          {"enableStun", a.enableStun},
          {"ipVersion", a.ipVersion},
          {"rport", a.rport},
          {"blfUri", a.blfUri}};
}

// Everything the dialog edits; id and active are left alone
static void fromMap(const QVariantMap &d, AccountRecord &a) {
  a.accountName = d["accountName"].toString();
  a.server = d["server"].toString().trimmed();
  a.proxy = d["proxy"].toString().trimmed();
  a.username = d["username"].toString().trimmed();
  a.domain = d["domain"].toString().trimmed();
  a.login = d["login"].toString().trimmed();
  a.password = d["password"].toString();
  a.displayName = d["displayName"].toString();
  a.transport = d["transport"].toString();
  a.registerRefresh = d["registerRefresh"].toInt();
  a.publishPresence = d["publishPresence"].toBool();
  a.iceEnabled = d["ice"].toBool();
  a.stunServer = d["stunServer"].toString().trimmed();
  a.turnServer = d["turnServer"].toString().trimmed();
  a.turnUser = d["turnUser"].toString();
  a.turnPass = d["turnPass"].toString();
  a.srtpMode = d["srtpMode"].toInt();
  a.allowIpRewrite = d["allowIpRewrite"].toBool();
  a.autoNat = d["autoNat"].toBool();
  a.enableStun = d["enableStun"].toBool();
  a.ipVersion = d["ipVersion"].toInt();
  a.rport = d["rport"].toBool();
  a.blfUri = d["blfUri"].toString().trimmed();
}

AccountModel &AccountModel::instance() {
  static AccountModel instance;
  return instance;
}

AccountModel::AccountModel(QObject *parent) : QAbstractListModel(parent) {
  // Emitted from the PJSIP thread; `this` as context queues it to ours
  connect(&SipService::instance(), &SipService::accountStateChanged, this,
          [this](QString id, bool active, int code, QString text) {
            QString status = active ? "Registered"
                             : code ? QString("%1 %2").arg(code).arg(text)
                                    : text;
            for (int i = 0; i < m_accounts.count(); ++i) {
              if (keyOf(m_accounts[i]) != id)
                continue;
              // The usual reason for a rejected login with nothing to send
              if (!active && (code == 401 || code == 403 || code == 407) &&
                  m_accounts[i].password.isEmpty())
                status += " (no password stored: is the keyring unlocked?)";
              m_status[id] = status;
              emit dataChanged(index(i), index(i), {StatusRole});
            }
          });

  // Global STUN, TURN and rewrite settings are baked into each account
  connect(&SipService::instance(), &SipService::networkSettingsApplied, this,
          [this] {
            for (const auto &rec : m_accounts) {
              if (rec.active)
                SipService::instance().registerAccount(keyOf(rec), toMap(rec));
            }
            emit defaultAccountIdChanged();
          });

  m_accounts = StorageService::instance().getAccounts();
  for (const auto &rec : m_accounts) {
    if (rec.active)
      SipService::instance().registerAccount(keyOf(rec), toMap(rec));
  }
}

QString AccountModel::defaultAccountId() const {
  QString first;
  for (const auto &acc : m_accounts) {
    if (!acc.active)
      continue;
    const QString id = keyOf(acc);
    if (id == m_selectedId)
      return id;
    if (first.isEmpty())
      first = id;
  }
  return first;
}

void AccountModel::selectNextAccount() {
  QStringList active;
  for (const auto &acc : m_accounts) {
    if (acc.active)
      active.append(keyOf(acc));
  }
  if (active.isEmpty())
    return;
  m_selectedId =
      active[(active.indexOf(defaultAccountId()) + 1) % active.size()];
  emit defaultAccountIdChanged();
}

int AccountModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_accounts.count();
}

QVariant AccountModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_accounts.count())
    return QVariant();

  const AccountRecord &a = m_accounts[index.row()];

  switch (role) {
  case IdRole:
    return QString::number(a.id);
  case AccountNameRole:
    return a.accountName;
  case ServerRole:
    return a.server;
  case ProxyRole:
    return a.proxy;
  case UsernameRole:
    return a.username;
  case DomainRole:
    return a.domain;
  case LoginRole:
    return a.login;
  case PasswordRole:
    return a.password;
  case DisplayNameRole:
    return a.displayName;
  case TransportRole:
    return a.transport;
  case RegisterRefreshRole:
    return a.registerRefresh;
  case PublishPresenceRole:
    return a.publishPresence;
  case IceRole:
    return a.iceEnabled;
  case StunServerRole:
    return a.stunServer;
  case TurnServerRole:
    return a.turnServer;
  case TurnUserRole:
    return a.turnUser;
  case TurnPassRole:
    return a.turnPass;
  case SrtpModeRole:
    return a.srtpMode;
  case AllowIpRewriteRole:
    return a.allowIpRewrite;
  case AutoNatRole:
    return a.autoNat;
  case EnableStunRole:
    return a.enableStun;
  case IpVersionRole:
    return a.ipVersion;
  case RportRole:
    return a.rport;
  case ActiveRole:
    return a.active;
  case BlfUriRole:
    return a.blfUri;
  case StatusRole:
    return m_status.value(keyOf(a));
  default:
    return QVariant();
  }
}

QHash<int, QByteArray> AccountModel::roleNames() const {
  return {{IdRole, "id"},
          {AccountNameRole, "accountName"},
          {ServerRole, "server"},
          {ProxyRole, "proxy"},
          {UsernameRole, "username"},
          {DomainRole, "domain"},
          {LoginRole, "login"},
          {PasswordRole, "password"},
          {DisplayNameRole, "displayName"},
          {TransportRole, "transport"},
          {RegisterRefreshRole, "registerRefresh"},
          {PublishPresenceRole, "publishPresence"},
          {IceRole, "ice"},
          {StunServerRole, "stunServer"},
          {TurnServerRole, "turnServer"},
          {TurnUserRole, "turnUser"},
          {TurnPassRole, "turnPass"},
          {SrtpModeRole, "srtpMode"},
          {AllowIpRewriteRole, "allowIpRewrite"},
          {AutoNatRole, "autoNat"},
          {EnableStunRole, "enableStun"},
          {IpVersionRole, "ipVersion"},
          {RportRole, "rport"},
          {ActiveRole, "active"},
          {BlfUriRole, "blfUri"},
          {StatusRole, "status"}};
}

// PJSIP frees an account's data when it is deleted, even under a live call
bool AccountModel::refuseIfBusy(const AccountRecord &rec) {
  if (!rec.active || !SipService::instance().hasActiveCalls(keyOf(rec)))
    return false;
  emit accountBusy(QString("%1 is in a call. Hang up first.").arg(keyOf(rec)));
  return true;
}

void AccountModel::addAccount(const QVariantMap &data) {
  AccountRecord rec{};
  fromMap(data, rec);
  rec.active = true;
  StorageService::instance().addAccount(rec);

  // Reload to pick up the database id
  beginResetModel();
  m_accounts = StorageService::instance().getAccounts();
  endResetModel();

  SipService::instance().registerAccount(keyOf(rec), toMap(rec));
  emit defaultAccountIdChanged();
}

void AccountModel::updateAccount(int index, const QVariantMap &data) {
  if (index < 0 || index >= m_accounts.count())
    return;

  AccountRecord &rec = m_accounts[index];
  const QString oldId = keyOf(rec);
  AccountRecord edited = rec;
  fromMap(data, edited);
  // Same identity is modified in place; a new one replaces the account
  if (keyOf(edited) != oldId && refuseIfBusy(rec))
    return;
  rec = edited;
  StorageService::instance().updateAccount(rec);
  emit dataChanged(this->index(index), this->index(index));

  if (rec.active) {
    if (oldId != keyOf(rec))
      SipService::instance().unregisterAccount(oldId);
    SipService::instance().registerAccount(keyOf(rec), toMap(rec));
  }
  emit defaultAccountIdChanged();
}

void AccountModel::removeAccount(int index) {
  if (index < 0 || index >= m_accounts.count())
    return;

  const AccountRecord rec = m_accounts[index];
  if (refuseIfBusy(rec))
    return;
  StorageService::instance().removeAccount(rec.id);
  if (rec.active)
    SipService::instance().unregisterAccount(keyOf(rec));

  beginRemoveRows(QModelIndex(), index, index);
  m_accounts.removeAt(index);
  endRemoveRows();
  emit defaultAccountIdChanged();
}

void AccountModel::toggleActive(int index) {
  if (index < 0 || index >= m_accounts.count())
    return;

  AccountRecord &rec = m_accounts[index];
  if (refuseIfBusy(rec)) // only an active account can be busy
    return;
  rec.active = !rec.active;
  StorageService::instance().updateAccount(rec);
  emit dataChanged(this->index(index), this->index(index), {ActiveRole});

  if (rec.active)
    SipService::instance().registerAccount(keyOf(rec), toMap(rec));
  else
    SipService::instance().unregisterAccount(keyOf(rec));
  emit defaultAccountIdChanged();
}

bool AccountModel::isDuplicate(int index, const QString &username,
                               const QString &domain) const {
  const QString id = username.trimmed() + "@" + domain.trimmed();
  for (int i = 0; i < m_accounts.count(); ++i) {
    if (i != index && keyOf(m_accounts[i]) == id)
      return true;
  }
  return false;
}

QVariantMap AccountModel::getAccount(int index) const {
  if (index < 0 || index >= m_accounts.count())
    return QVariantMap();
  return toMap(m_accounts[index]);
}
