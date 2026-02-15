#ifndef CONTACTSMODEL_H
#define CONTACTSMODEL_H

#include <QAbstractListModel>
#include <QList>

struct Contact {
  QString id;
  QString name;
  QString number;
  QString status; // Online, Offline, Busy
};

class ContactsModel : public QAbstractListModel {
  Q_OBJECT
public:
  enum ContactRoles {
    IdRole = Qt::UserRole + 1,
    NameRole,
    NumberRole,
    StatusRole
  };

  static ContactsModel &instance();
  explicit ContactsModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  Q_INVOKABLE void addContact(const QString &name, const QString &number);
  Q_INVOKABLE void removeContact(int index);
  void updateContactStatus(const QString &number, const QString &status);

private:
  QList<Contact> m_contacts;
};

#endif // CONTACTSMODEL_H
