#include <QGuiApplication>
#include <QQmlApplicationEngine>

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
