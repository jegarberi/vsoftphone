#include <QDir>
#include <QGuiApplication>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QStandardPaths>

#include "QmlSetup.h"
#include "pjsip_wrapper/SipService.h"
#include "pjsip_wrapper/StorageService.h"

int main(int argc, char *argv[]) {
  // PJSIP's SDL video backend would otherwise swallow SIGINT/SIGTERM
  qputenv("SDL_NO_SIGNAL_HANDLERS", "1");

  QGuiApplication app(argc, argv);

  app.setOrganizationName("Antigravity");
  app.setOrganizationDomain("antigravity.dev");
  app.setApplicationName("MicrosipClone");

  // A second copy would register the same accounts from another port
  const QString dataDir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir().mkpath(dataDir);
  QLockFile instanceLock(dataDir + "/instance.lock");
  instanceLock.setStaleLockTime(0);
  if (!instanceLock.tryLock()) {
    qCritical("MicrosipClone is already running");
    return 1;
  }

  // Initialize SipService and StorageService
  StorageService::instance().init();
  SipService::instance().init();

  QQmlApplicationEngine engine;
  setupQml(engine);

  const QUrl url(QStringLiteral("qrc:/ui/main.qml"));
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
