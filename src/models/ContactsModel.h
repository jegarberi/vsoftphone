#ifndef CONTACTSMODEL_H
#define CONTACTSMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QMap>

struct Contact {
  QString id;
  QString name;
  QString number;
  QString group;
  QString status; // Online, Offline, or a lamp: Idle, Ringing, Busy
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
  // Busy lamp state of a monitored user. Sets the status of the matching
  // contact, or lists the user under "BLF" when there is no such contact.
  void updateLamp(const QString &accountId, const QString &uri,
                  const QString &name, const QString &state);
  // Forgets the lamps that came from this account's subscription
  void clearLamps(const QString &accountId);

private:
  explicit ContactsModel(QObject *parent = nullptr);
  void reload();
  void subscribePresence();

  int rowOfLamp(const QString &uri) const;
  Contact lampRow(const QString &uri) const;

  QList<Contact> m_contacts;
  struct Lamp {
    QString accountId;
    QString name;
    QString state;
  };
  QMap<QString, Lamp> m_lamps; // bare "user@domain" -> last known state
  QString m_presenceAccount;   // the account presence is subscribed through
};

#endif // CONTACTSMODEL_H
