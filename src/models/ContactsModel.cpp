#include "ContactsModel.h"
#include <algorithm>
#include "../pjsip_wrapper/BlfService.h"
#include "../pjsip_wrapper/BuddyService.h"
#include "../pjsip_wrapper/SipUri.h"
#include "../pjsip_wrapper/StorageService.h"
#include "AccountModel.h"

ContactsModel &ContactsModel::instance() {
  static ContactsModel _instance;
  return _instance;
}

ContactsModel::ContactsModel(QObject *parent) : QAbstractListModel(parent) {
  // Accounts own the presence subscriptions, so redo them when accounts change
  connect(&AccountModel::instance(), &AccountModel::defaultAccountIdChanged,
          this, &ContactsModel::subscribePresence);
  connect(&BlfService::instance(), &BlfService::lampChanged, this,
          &ContactsModel::updateLamp);
  connect(&BlfService::instance(), &BlfService::lampsCleared, this,
          &ContactsModel::clearLamps);
  reload();
}

void ContactsModel::reload() {
  beginResetModel();
  m_contacts.clear();
  QList<ContactRecord> records = StorageService::instance().getContacts();
  for (const auto &rec : records) {
    m_contacts.append(
        {QString::number(rec.id), rec.name, rec.number, rec.group, "Offline"});
  }
  for (auto it = m_lamps.cbegin(); it != m_lamps.cend(); ++it) {
    const int row = rowOfLamp(it.key());
    if (row >= 0)
      m_contacts[row].status = it->state;
    else
      m_contacts.append(lampRow(it.key()));
  }
  endResetModel();
  subscribePresence();
}

// The contact a lamp belongs to: stored as the full URI or as its user part
int ContactsModel::rowOfLamp(const QString &uri) const {
  const QString user = uri.section('@', 0, 0);
  for (int i = 0; i < m_contacts.count(); ++i) {
    const QString number = bareSipUri(m_contacts[i].number);
    if (number == uri || number == user)
      return i;
  }
  return -1;
}

// A monitored user nobody saved as a contact gets a row of its own
Contact ContactsModel::lampRow(const QString &uri) const {
  const Lamp &lamp = m_lamps[uri];
  return {QString(), lamp.name.isEmpty() ? uri.section('@', 0, 0) : lamp.name,
          uri, "BLF", lamp.state};
}

void ContactsModel::updateLamp(const QString &accountId, const QString &uri,
                               const QString &name, const QString &state) {
  m_lamps[uri] = {accountId, name, state};
  const int row = rowOfLamp(uri);
  if (row >= 0) {
    m_contacts[row].status = state;
    if (m_contacts[row].id.isEmpty())
      m_contacts[row].name = lampRow(uri).name;
    emit dataChanged(index(row), index(row), {StatusRole, NameRole});
  } else {
    beginInsertRows(QModelIndex(), m_contacts.count(), m_contacts.count());
    m_contacts.append(lampRow(uri));
    endInsertRows();
  }
}

void ContactsModel::clearLamps(const QString &accountId) {
  bool removed = false;
  for (auto it = m_lamps.begin(); it != m_lamps.end();) {
    if (it->accountId == accountId) {
      it = m_lamps.erase(it);
      removed = true;
    } else {
      ++it;
    }
  }
  if (removed)
    reload();
}

void ContactsModel::subscribePresence() {
  BuddyService &buddies = BuddyService::instance();
  const QString accountId = AccountModel::instance().defaultAccountId();
  int stored = 0;
  for (const auto &contact : m_contacts)
    stored += !contact.id.isEmpty();
  // Already subscribed through this account and nothing was dropped
  if (accountId == m_presenceAccount && buddies.count() == stored)
    return;

  buddies.clear();
  m_presenceAccount = accountId;
  if (accountId.isEmpty())
    return;
  for (const auto &contact : m_contacts) {
    if (!contact.id.isEmpty()) // BLF rows already have their own subscription
      buddies.addBuddy(accountId, contact.number);
  }
}

int ContactsModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_contacts.count();
}

QVariant ContactsModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_contacts.count())
    return QVariant();

  const Contact &contact = m_contacts[index.row()];

  switch (role) {
  case IdRole:
    return contact.id;
  case NameRole:
    return contact.name;
  case NumberRole:
    return contact.number;
  case GroupRole:
    return contact.group;
  case StatusRole:
    return contact.status;
  default:
    return QVariant();
  }
}

QHash<int, QByteArray> ContactsModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[IdRole] = "id";
  roles[NameRole] = "name";
  roles[NumberRole] = "number";
  roles[GroupRole] = "group";
  roles[StatusRole] = "status";
  return roles;
}

void ContactsModel::addContact(const QString &name, const QString &number,
                               const QString &group) {
  StorageService::instance().addContact(name, number, group);
  reload(); // picks up the database id and the group ordering
}

void ContactsModel::removeContact(int index) {
  if (index < 0 || index >= m_contacts.count())
    return;

  if (m_contacts[index].id.isEmpty())
    return; // a BLF row comes from the server, not from the database
  int id = m_contacts[index].id.toInt();
  StorageService::instance().removeContact(id);
  BuddyService::instance().removeBuddy(m_contacts[index].number);

  beginRemoveRows(QModelIndex(), index, index);
  m_contacts.removeAt(index);
  endRemoveRows();
}

void ContactsModel::updateContactStatus(const QString &number,
                                        const QString &status) {
  for (int i = 0; i < m_contacts.count(); ++i) {
    if (m_contacts[i].number == number) {
      // A busy lamp says more than presence does; do not overwrite it
      const QString bare = bareSipUri(number);
      if (m_lamps.contains(bare) ||
          std::any_of(m_lamps.keyBegin(), m_lamps.keyEnd(),
                      [&](const QString &uri) {
                        return uri.section('@', 0, 0) == bare;
                      }))
        break;
      if (m_contacts[i].status != status) {
        m_contacts[i].status = status;
        emit dataChanged(index(i), index(i), {StatusRole});
      }
      break;
    }
  }
}
