#include "PresenceService.h"
#include "SipService.h"
#include <QDebug>
#include <QSettings>

PresenceService &PresenceService::instance() {
  static PresenceService _instance;
  return _instance;
}

PresenceService::PresenceService(QObject *parent)
    : QObject(parent),
      m_status(QSettings().value("presence/status", "Online").toString()) {}

void PresenceService::setStatus(const QString &newStatus) {
  if (m_status == newStatus)
    return;

  m_status = newStatus;
  QSettings().setValue("presence/status", m_status);
  emit statusChanged(m_status);
  apply();
}

void PresenceService::apply() {
  pj::PresenceStatus ps;
  if (m_status == "Online") {
    ps.status = PJSUA_BUDDY_STATUS_ONLINE;
  } else if (m_status == "Offline") {
    ps.status = PJSUA_BUDDY_STATUS_OFFLINE;
  } else {
    ps.status = PJSUA_BUDDY_STATUS_ONLINE;
    ps.note = m_status.toStdString();
  }

  const auto accounts = SipService::instance().getAccounts();
  for (const auto &acc : accounts) {
    try {
      acc->setOnlineStatus(ps);
    } catch (pj::Error &err) {
      qWarning() << "Error setting online status for account:"
                 << QString::fromStdString(err.info());
    }
  }
}
