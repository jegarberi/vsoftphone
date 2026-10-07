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
  for (auto it = m_lamps.cbegin(); it != m_lamps.cend(); ++it)
    applyLamp(it.key());
  endResetModel();
  subscribePresence();
}

// Puts a known lamp state on its contact, adding a "BLF" row if there is none.
// Returns the row; the caller announces the change.
int ContactsModel::applyLamp(const QString &uri) {
  const Lamp &lamp = m_lamps[uri];
  const QString user = uri.section('@', 0, 0);
  for (int i = 0; i < m_contacts.count(); ++i) {
    const QString number = bareSipUri(m_contacts[i].number);
    if (number == uri || number == user) {
      m_contacts[i].status = lamp.state;
      return i;
    }
  }
  m_contacts.append(
      {QString(), lamp.name.isEmpty() ? user : lamp.name, uri, "BLF", lamp.state});
  return m_contacts.count() - 1;
}

void ContactsModel::updateLamp(const QString &uri, const QString &name,
                               const QString &state) {
  m_lamps[uri] = {name, state};
  // Simplest correct way to cover both an updated row and an appended one
  beginResetModel();
  applyLamp(uri);
  endResetModel();
}

void ContactsModel::subscribePresence() {
  BuddyService &buddies = BuddyService::instance();
  buddies.clear();
  const QString accountId = AccountModel::instance().defaultAccountId();
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
