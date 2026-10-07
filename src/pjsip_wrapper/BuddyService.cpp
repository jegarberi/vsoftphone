#include "BuddyService.h"
#include "../models/ContactsModel.h"
#include "SipService.h"
#include "SipUri.h"
#include <QDebug>

using namespace pj;

// May run on the PJSIP thread; the model is updated on the GUI thread
void MyBuddy::onBuddyState() {
  BuddyInfo bi = getInfo();
  const QString status =
      bi.presStatus.status == PJSUA_BUDDY_STATUS_ONLINE ? "Online" : "Offline";
  const QString contact = number;
  QMetaObject::invokeMethod(
      &ContactsModel::instance(),
      [contact, status] {
        ContactsModel::instance().updateContactStatus(contact, status);
      },
      Qt::QueuedConnection);
}

BuddyService &BuddyService::instance() {
  static BuddyService _instance;
  return _instance;
}

BuddyService::BuddyService(QObject *parent) : QObject(parent) {}

void BuddyService::addBuddy(const QString &accountId, const QString &number) {
  auto acc = SipService::instance().getAccount(accountId);
  if (!acc)
    return;

  try {
    QString uri = bareSipUri(number);
    if (!uri.contains('@'))
      uri += accountId.mid(accountId.indexOf('@'));

    BuddyConfig cfg;
    cfg.uri = ("sip:" + uri).toStdString();
    cfg.subscribe = true;

    auto buddy = std::make_shared<MyBuddy>();
    buddy->number = number;
    buddy->create(*acc, cfg);
    m_buddies[number] = buddy;
  } catch (Error &err) {
    qWarning() << "Error adding buddy:" << QString::fromStdString(err.info());
  }
}

void BuddyService::removeBuddy(const QString &number) {
  m_buddies.remove(number);
}
