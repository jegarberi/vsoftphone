#include "QmlSetup.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>

#include "models/AccountModel.h"
#include "models/CallModel.h"
#include "models/ContactsModel.h"
#include "models/HistoryModel.h"
#include "models/MessageModel.h"
#include "pjsip_wrapper/BuddyService.h"
#include "pjsip_wrapper/CallService.h"
#include "pjsip_wrapper/HardwareService.h"
#include "pjsip_wrapper/PresenceService.h"
#include "pjsip_wrapper/SipService.h"
#include "pjsip_wrapper/StorageService.h"
#include "ui/Theme.h"

void setupQml(QQmlEngine &engine) {
  // The UI restyles its controls, which the native Windows and macOS styles
  // do not allow. Fusion is what Linux desktops get by default anyway.
  if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE"))
    QQuickStyle::setStyle("Fusion");

  qmlRegisterType<HistoryModel>("MicrosipClone", 1, 0, "HistoryModel");
  qmlRegisterType<MessageModel>("MicrosipClone", 1, 0, "MessageModel");

  QQmlContext *ctx = engine.rootContext();
  ctx->setContextProperty("hardwareService", &HardwareService::instance());
  ctx->setContextProperty("presenceService", &PresenceService::instance());
  ctx->setContextProperty("callService", &CallService::instance());
  ctx->setContextProperty("sipService", &SipService::instance());
  ctx->setContextProperty("storageService", &StorageService::instance());
  ctx->setContextProperty("buddyService", &BuddyService::instance());
  ctx->setContextProperty("accountModel", &AccountModel::instance());
  ctx->setContextProperty("contactsModel", &ContactsModel::instance());
  ctx->setContextProperty("callModel", &CallModel::instance());
  ctx->setContextProperty("Theme", &Theme::instance());
}
