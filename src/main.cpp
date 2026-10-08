#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QWindow>
#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/private/qandroidextras_p.h>
// PJSIP's audio device talks to Android through Java and expects the VM here.
// It normally learns it in JNI_OnLoad, which Android does not call for a
// library Qt loads the way it loads the application.
extern "C" JavaVM *pj_jni_jvm;
#endif
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#endif

#include "QmlSetup.h"
#include "pjsip_wrapper/CallService.h"
#include "pjsip_wrapper/SipService.h"
#include "pjsip_wrapper/StorageService.h"
#include "ui/Theme.h"

int main(int argc, char *argv[]) {
  // PJSIP's SDL video backend would otherwise swallow SIGINT/SIGTERM
  qputenv("SDL_NO_SIGNAL_HANDLERS", "1");

  // The interface zoom from Settings; Qt reads it when the application starts
  const double scale = Theme::savedUiScale();
  if (scale > 0 && !qFuzzyCompare(scale, 1.0) &&
      qEnvironmentVariableIsEmpty("QT_SCALE_FACTOR"))
    qputenv("QT_SCALE_FACTOR", QByteArray::number(scale, 'f', 2));

  QGuiApplication app(argc, argv);

  app.setOrganizationName("Antigravity");
  app.setOrganizationDomain("antigravity.dev");
  app.setApplicationName("MicrosipClone");
  app.setWindowIcon(QIcon(":/ui/icon.png"));
  // The main window decides: it quits, or hides in the tray (see main.qml)
  app.setQuitOnLastWindowClosed(false);

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

#ifdef Q_OS_ANDROID
  pj_jni_jvm = QJniEnvironment::javaVM();
  // Without the microphone there are no calls; ask before PJSIP opens it
  QtAndroidPrivate::requestPermission(
      QStringLiteral("android.permission.RECORD_AUDIO"))
      .waitForFinished();
  // Android 13 asks before showing the call notifications below
  QtAndroidPrivate::requestPermission(
      QStringLiteral("android.permission.POST_NOTIFICATIONS"))
      .waitForFinished();
  // Keeps the app registered while it is not on screen, and announces the
  // calls that arrive then (android/.../PhoneService.java)
  const char *phoneService = "dev/antigravity/microsipclone/PhoneService";
  const QJniObject context(QNativeInterface::QAndroidApplication::context());
  QJniObject::callStaticMethod<void>(phoneService, "start",
                                     "(Landroid/content/Context;)V",
                                     context.object());
  QObject::connect(&CallService::instance(), &CallService::incomingCall, &app,
                   [=](int, const QString &from) {
                     QJniObject::callStaticMethod<void>(
                         phoneService, "incomingCall",
                         "(Landroid/content/Context;Ljava/lang/String;)V",
                         context.object(),
                         QJniObject::fromString(from).object());
                   });
  QObject::connect(&CallService::instance(), &CallService::callStateChanged,
                   &app, [=](int, const QString &state) {
                     if (state == "CONFIRMED" || state == "DISCONNECTED")
                       QJniObject::callStaticMethod<void>(
                           phoneService, "callsOver",
                           "(Landroid/content/Context;)V", context.object());
                   });
#endif

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
