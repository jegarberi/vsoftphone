#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QWindow>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#endif

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
  // Held until the process ends, however it ends, so it can never go stale
#ifdef Q_OS_WIN
  ::CreateMutexW(nullptr, TRUE, L"Local\\AntigravityMicrosipClone");
  const bool alreadyRunning = ::GetLastError() == ERROR_ALREADY_EXISTS;
#else
  const int lockFd =
      ::open(QFile::encodeName(dataDir + "/instance.lock").constData(),
             O_RDWR | O_CREAT | O_CLOEXEC, 0600);
  const bool alreadyRunning =
      lockFd < 0 || ::flock(lockFd, LOCK_EX | LOCK_NB) != 0;
#endif
  // The running copy listens here so that a second launch brings it forward
#ifdef Q_OS_WIN
  const QString activation =
      "antigravity-microsipclone-" + qEnvironmentVariable("USERNAME");
#else
  const QString activation = dataDir + "/activate.sock";
#endif
  if (alreadyRunning) {
    QLocalSocket running;
    running.connectToServer(activation);
    running.waitForConnected(1000);
    return 0;
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

  QLocalServer::removeServer(activation); // left behind by a crash
  QLocalServer activationServer;
  activationServer.listen(activation);
  QObject::connect(&activationServer, &QLocalServer::newConnection, &app, [&] {
    while (QLocalSocket *knock = activationServer.nextPendingConnection())
      knock->deleteLater();
    const auto roots = engine.rootObjects();
    for (QObject *root : roots) {
      if (auto *window = qobject_cast<QWindow *>(root)) {
        window->show();
        window->raise();
        window->requestActivate();
      }
    }
  });

  return app.exec();
}
