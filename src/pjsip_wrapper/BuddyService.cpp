#include "BuddyService.h"
#include "../models/ContactsModel.h"
#include "SipService.h"
#include <QDebug>

using namespace pj;

void MyBuddy::onBuddyState() {
  BuddyInfo bi = getInfo();
  ContactsModel::instance().updateContactStatus(
      QString::fromStdString(bi.uri),
      QString::fromStdString(bi.presStatus.statusText));
}

BuddyService &BuddyService::instance() {
  static BuddyService _instance;
  return _instance;
}

BuddyService::BuddyService(QObject *parent) : QObject(parent) {}

void BuddyService::addBuddy(const QString &accountId, const QString &uri) {
  auto acc = SipService::instance().getAccount(accountId);
  if (!acc) {
    qWarning() << "Account not found for adding buddy:" << accountId;
    return;
  }

  try {
    BuddyConfig cfg;
    cfg.uri = uri.toStdString();
    cfg.subscribe = true;

    auto buddy = std::make_shared<MyBuddy>();
    buddy->create(*acc, cfg);
    m_buddies[uri] = buddy;

    // ContactsModel already has the contact, or it will be added?
    // For now assume it exists or will be added via UI.
    qDebug() << "Added buddy subscription for:" << uri;
  } catch (Error &err) {
    qWarning() << "Error adding buddy:" << QString::fromStdString(err.info());
  }
}

void BuddyService::removeBuddy(const QString &uri) {
  if (m_buddies.contains(uri)) {
    m_buddies.remove(uri);
    qDebug() << "Removed buddy subscription for:" << uri;
  }
}
