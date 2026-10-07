#ifndef BUDDYSERVICE_H
#define BUDDYSERVICE_H

#include <QMap>
#include <QObject>
#include <memory>
#include <pjsua2.hpp>

class MyBuddy : public pj::Buddy {
public:
  MyBuddy() {}
  virtual void onBuddyState() override;

  QString number; // the contact's number as stored in ContactsModel
};

class BuddyService : public QObject {
  Q_OBJECT
public:
  static BuddyService &instance();

  void addBuddy(const QString &accountId, const QString &number);
  void removeBuddy(const QString &number);
  // Must run before any pj::Account is destroyed: the account deletes the
  // buddies attached to it.
  void clear() { m_buddies.clear(); }
  int count() const { return m_buddies.size(); }

private:
  explicit BuddyService(QObject *parent = nullptr);
  QMap<QString, std::shared_ptr<MyBuddy>> m_buddies;
};

#endif // BUDDYSERVICE_H
