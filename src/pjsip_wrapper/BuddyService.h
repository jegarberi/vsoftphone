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
};

class BuddyService : public QObject {
  Q_OBJECT
public:
  static BuddyService &instance();

  void addBuddy(const QString &accountId, const QString &uri);
  void removeBuddy(const QString &uri);

private:
  explicit BuddyService(QObject *parent = nullptr);
  QMap<QString, std::shared_ptr<MyBuddy>> m_buddies;
};

#endif // BUDDYSERVICE_H
