#ifndef CONTACTSMODEL_H
#define CONTACTSMODEL_H

#include <QAbstractListModel>
#include <QList>

struct Contact {
  QString id;
  QString name;
  QString number;
  QString group;
  QString status; // Online, Offline
};

class ContactsModel : public QAbstractListModel {
  Q_OBJECT
public:
  enum ContactRoles {
    IdRole = Qt::UserRole + 1,
    NameRole,
    NumberRole,
    GroupRole,
    StatusRole
  };

  static ContactsModel &instance();

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  Q_INVOKABLE void addContact(const QString &name, const QString &number,
                              const QString &group = QString());
  Q_INVOKABLE void removeContact(int index);
  void updateContactStatus(const QString &number, const QString &status);

private:
  explicit ContactsModel(QObject *parent = nullptr);
  void reload();
  void subscribePresence();

  QList<Contact> m_contacts;
};

#endif // CONTACTSMODEL_H
