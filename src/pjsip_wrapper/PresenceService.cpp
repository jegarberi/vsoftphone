#include "PresenceService.h"
#include "SipService.h"
#include <QDebug>

PresenceService &PresenceService::instance() {
  static PresenceService _instance;
  return _instance;
}

PresenceService::PresenceService(QObject *parent)
    : QObject(parent), m_status("Online") {}

void PresenceService::setStatus(const QString &newStatus) {
  if (m_status == newStatus)
    return;

  m_status = newStatus;
  emit statusChanged(m_status);

  // Update all accounts
  pj::PresenceStatus ps;
  if (newStatus == "Online") {
    ps.status = PJSUA_BUDDY_STATUS_ONLINE;
  } else if (newStatus == "Offline") {
    ps.status = PJSUA_BUDDY_STATUS_OFFLINE;
  } else {
    ps.status = PJSUA_BUDDY_STATUS_ONLINE;
    ps.note = newStatus.toStdString();
  }

  // Iterate over managed accounts in SipService and set status
  auto accounts = SipService::instance().getAccounts();
  for (auto acc : accounts) {
    try {
      acc->setOnlineStatus(ps);
    } catch (pj::Error &err) {
      qWarning() << "Error setting online status for account:"
                 << QString::fromStdString(err.info());
    }
  }
}
