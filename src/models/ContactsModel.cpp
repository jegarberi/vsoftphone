#include "ContactsModel.h"
#include "../pjsip_wrapper/StorageService.h"

ContactsModel &ContactsModel::instance() {
  static ContactsModel _instance;
  return _instance;
}

ContactsModel::ContactsModel(QObject *parent) : QAbstractListModel(parent) {
  QList<ContactRecord> records = StorageService::instance().getContacts();
  for (const auto &rec : records) {
    m_contacts.append(
        {QString::number(rec.id), rec.name, rec.number, "Offline"});
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
  roles[StatusRole] = "status";
  return roles;
}

void ContactsModel::addContact(const QString &name, const QString &number) {
  StorageService::instance().addContact(name, number);

  // Refresh model is easier or we find the ID?
  // For simplicity refresh from DB or just add with a temporary ID?
  // Let's just reset model for a full refresh to get the real ID from DB
  beginResetModel();
  m_contacts.clear();
  QList<ContactRecord> records = StorageService::instance().getContacts();
  for (const auto &rec : records) {
    m_contacts.append(
        {QString::number(rec.id), rec.name, rec.number, "Offline"});
  }
  endResetModel();
}

void ContactsModel::removeContact(int index) {
  if (index < 0 || index >= m_contacts.count())
    return;

  int id = m_contacts[index].id.toInt();
  StorageService::instance().removeContact(id);

  beginRemoveRows(QModelIndex(), index, index);
  m_contacts.removeAt(index);
  endRemoveRows();
}

void ContactsModel::updateContactStatus(const QString &number,
                                        const QString &status) {
  for (int i = 0; i < m_contacts.count(); ++i) {
    if (m_contacts[i].number == number) {
      if (m_contacts[i].status != status) {
        m_contacts[i].status = status;
        emit dataChanged(index(i), index(i), {StatusRole});
      }
      break;
    }
  }
}
