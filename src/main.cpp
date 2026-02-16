#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>

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

int main(int argc, char *argv[]) {
  QGuiApplication app(argc, argv);

  app.setOrganizationName("Antigravity");
  app.setOrganizationDomain("antigravity.dev");
  app.setApplicationName("MicrosipClone");

  // Initialize SipService and StorageService
  StorageService::instance().init();
  SipService::instance().init();

  qmlRegisterType<ContactsModel>("MicrosipClone", 1, 0, "ContactsModel");
  qmlRegisterType<HistoryModel>("MicrosipClone", 1, 0, "HistoryModel");
  qmlRegisterType<MessageModel>("MicrosipClone", 1, 0, "MessageModel");

  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("hardwareService",
                                           &HardwareService::instance());
  engine.rootContext()->setContextProperty("presenceService",
                                           &PresenceService::instance());
  engine.rootContext()->setContextProperty("callService",
                                           &CallService::instance());
  engine.rootContext()->setContextProperty("sipService",
                                           &SipService::instance());
  engine.rootContext()->setContextProperty("storageService",
                                           &StorageService::instance());
  engine.rootContext()->setContextProperty("buddyService",
                                           &BuddyService::instance());
  engine.rootContext()->setContextProperty("accountModel",
                                           &AccountModel::instance());
  engine.rootContext()->setContextProperty("contactsModel",
                                           &ContactsModel::instance());
  engine.rootContext()->setContextProperty("callModel", &CallModel::instance());
  engine.rootContext()->setContextProperty("Theme", &Theme::instance());

  const QUrl url(u"qrc:/ui/main.qml"_qs);
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreated, &app,
      [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
          QCoreApplication::exit(-1);
      },
      Qt::QueuedConnection);
  engine.load(url);

  return app.exec();
}
