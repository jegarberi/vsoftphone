// End-to-end check of the service layer without a SIP server: the endpoint
// calls and messages its own UDP port, with the null audio device.
//
// Run: ctest --preset conan-release   (or build/Release/call_flow_test)

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <pjsua-lib/pjsua.h>

#include "QmlSetup.h"
#include "models/AccountModel.h"
#include "models/CallModel.h"
#include "models/ContactsModel.h"
#include "models/HistoryModel.h"
#include "models/MessageModel.h"
#include "pjsip_wrapper/CallService.h"
#include "pjsip_wrapper/SipService.h"
#include "pjsip_wrapper/StorageService.h"

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
      std::fflush(stderr);                                                     \
      std::_Exit(1);                                                           \
    }                                                                          \
  } while (0)

static bool waitFor(const std::function<bool()> &cond, int timeoutMs = 8000) {
  for (int waited = 0; waited < timeoutMs; waited += 10) {
    QCoreApplication::processEvents();
    if (cond())
      return true;
    QThread::msleep(10);
  }
  return false;
}

static void pump(int ms) {
  waitFor([] { return false; }, ms);
}

static QString callState(int row) {
  CallModel &calls = CallModel::instance();
  return calls.data(calls.index(row), CallModel::StateRole).toString();
}

static int callId(int row) {
  CallModel &calls = CallModel::instance();
  return calls.data(calls.index(row), CallModel::IdRole).toInt();
}

static bool allConfirmed(int expected) {
  if (CallModel::instance().rowCount() != expected)
    return false;
  for (int i = 0; i < expected; ++i) {
    if (callState(i) != "CONFIRMED")
      return false;
  }
  return true;
}

static pjsua_call_media_status mediaStatus(int id) {
  pjsua_call_info ci;
  CHECK(pjsua_call_get_info(id, &ci) == PJ_SUCCESS);
  return ci.media_cnt ? ci.media[0].status : PJSUA_CALL_MEDIA_NONE;
}

// True while the microphone (conference port 0) feeds the call
static bool micReaches(int id) {
  pjsua_call_info ci;
  CHECK(pjsua_call_get_info(id, &ci) == PJ_SUCCESS);
  pjsua_conf_port_info mic;
  CHECK(pjsua_conf_get_port_info(0, &mic) == PJ_SUCCESS);
  for (unsigned i = 0; i < mic.listener_cnt; ++i) {
    if (mic.listeners[i] == ci.media[0].stream.aud.conf_slot)
      return true;
  }
  return false;
}

// QML warnings (binding errors, missing properties) all name a qrc: file
static QStringList qmlWarnings;
static void collectQmlWarnings(QtMsgType type, const QMessageLogContext &,
                               const QString &msg) {
  if (type != QtDebugMsg && type != QtInfoMsg && msg.contains("qrc:"))
    qmlWarnings.append(msg);
  std::fprintf(stderr, "%s\n", qPrintable(msg));
}

// ICE state of the call's audio transport; nullptr when ICE is not in use
static const pjmedia_ice_transport_info *iceInfo(int id,
                                                 pjmedia_transport_info &tp) {
  pjmedia_transport_info_init(&tp);
  if (pjsua_call_get_med_transport_info(id, 0, &tp) != PJ_SUCCESS)
    return nullptr;
  return static_cast<const pjmedia_ice_transport_info *>(
      pjmedia_transport_info_get_spc_info(&tp, PJMEDIA_TRANSPORT_TYPE_ICE));
}

static bool iceRunning(int id) {
  pjmedia_transport_info tp;
  const pjmedia_ice_transport_info *ice = iceInfo(id, tp);
  return ice && ice->active && ice->sess_state == PJ_ICE_STRANS_STATE_RUNNING;
}

static int historyCount(const QString &direction) {
  int n = 0;
  for (const auto &rec : StorageService::instance().getCallHistory())
    n += rec.direction == direction;
  return n;
}

int main(int argc, char *argv[]) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  qputenv("SDL_NO_SIGNAL_HANDLERS", "1");
  QStandardPaths::setTestModeEnabled(true); // keep away from the real profile
  QGuiApplication app(argc, argv);
  app.setOrganizationName("AntigravityTest");
  app.setApplicationName("MicrosipCloneTest");

  const QString dataDir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  CHECK(dataDir.contains("qttest"));
  QDir(dataDir).removeRecursively();
  QSettings().clear();

  CHECK(StorageService::instance().init());
  SipService &sip = SipService::instance();
  sip.init();
  CHECK(sip.endpoint());
  sip.endpoint()->audDevManager().setNullDev();

  pjsua_transport_info ti;
  CHECK(pjsua_transport_get_info(0, &ti) == PJ_SUCCESS);
  const QString domain = QString("127.0.0.1:%1").arg(ti.local_name.port);
  const QString accountId = "tester@" + domain;

  // --- account -------------------------------------------------------------
  AccountModel &accounts = AccountModel::instance();
  QVariantMap account{{"accountName", "loopback"}, {"username", "tester"},
                      {"domain", domain},          {"password", "secret"},
                      {"transport", "UDP"},        {"enableStun", false},
                      {"ipVersion", 0}};
  accounts.addAccount(account);
  CHECK(accounts.rowCount() == 1);
  CHECK(accounts.defaultAccountId() == accountId);
  CHECK(sip.getAccount(accountId)); // registered right after being added
  // Nothing answers REGISTER here, so a failure status must reach the model
  CHECK(waitFor([&] {
    return !accounts.data(accounts.index(0), AccountModel::StatusRole)
                .toString()
                .isEmpty();
  }, 40000));

  // --- contacts and presence subscription ----------------------------------
  ContactsModel &contacts = ContactsModel::instance();
  contacts.addContact("Tester", "tester", "Team");
  CHECK(contacts.rowCount() == 1);
  CHECK(contacts.data(contacts.index(0), ContactsModel::GroupRole) == "Team");

  // --- answered call: mute, hold, resume, record, DTMF ---------------------
  CallService &callService = CallService::instance();
  HistoryModel history;
  sip.setAutoAnswer(true);
  callService.makeCall(accountId, "sip:tester");
  CHECK(waitFor([] { return allConfirmed(2); }));
  const int outgoing = callId(0);
  CHECK(waitFor([&] {
    return mediaStatus(outgoing) == PJSUA_CALL_MEDIA_ACTIVE;
  }));

  CHECK(waitFor([&] { return micReaches(outgoing); }));
  callService.toggleMute(outgoing);
  CHECK(!micReaches(outgoing));

  callService.holdCall(outgoing);
  CHECK(waitFor([&] {
    return mediaStatus(outgoing) == PJSUA_CALL_MEDIA_LOCAL_HOLD;
  }));
  callService.resumeCall(outgoing);
  CHECK(waitFor([&] {
    return mediaStatus(outgoing) == PJSUA_CALL_MEDIA_ACTIVE;
  }));
  pump(200);
  CHECK(!micReaches(outgoing)); // mute survives the re-INVITE
  callService.toggleMute(outgoing);
  CHECK(micReaches(outgoing));

  CHECK(callService.toggleRecording(outgoing));
  pump(500);
  CHECK(!callService.toggleRecording(outgoing));
  const QFileInfoList recordings =
      QDir(dataDir + "/recordings").entryInfoList({"*.wav"}, QDir::Files);
  CHECK(recordings.size() == 1);
  CHECK(recordings.first().size() > 44); // more than a bare WAV header

  callService.sendDtmf(outgoing, "12#");
  pump(1200); // let the call accumulate a non-zero connected duration
  callService.hangupCall(outgoing);
  CHECK(waitFor([] { return CallModel::instance().rowCount() == 0; }));
  CHECK(historyCount("Outgoing") == 1);
  CHECK(historyCount("Incoming") == 1);
  CHECK(historyCount("Missed") == 0);
  CHECK(history.rowCount() == 2); // the model followed the database

  // --- video call is still answered (audio only on the answering side) ------
  callService.makeCall(accountId, "tester", true);
  CHECK(waitFor([] { return allConfirmed(2); }));
  callService.hangupCall(callId(0));
  CHECK(waitFor([] { return CallModel::instance().rowCount() == 0; }));

  // --- unanswered call is recorded as missed -------------------------------
  sip.setAutoAnswer(false);
  callService.makeCall(accountId, "tester");
  CHECK(waitFor([] { return CallModel::instance().rowCount() == 2; }));
  pump(300);
  CHECK(callState(1) != "CONFIRMED");
  callService.hangupCall(callId(0));
  CHECK(waitFor([] { return CallModel::instance().rowCount() == 0; }));
  CHECK(historyCount("Missed") == 1);

  // --- do not disturb: caller gets 486, nothing rings, logged as missed ----
  int incomingSignals = 0, lastFinalStatus = 0;
  QObject::connect(&callService, &CallService::incomingCall, &app,
                   [&] { ++incomingSignals; });
  QObject::connect(&callService, &CallService::callStateChanged, &app,
                   [&](int, QString state, int status) {
                     if (state == "DISCONNECTED")
                       lastFinalStatus = status;
                   });
  const int outgoingBefore = historyCount("Outgoing");
  sip.setDoNotDisturb(true);
  callService.makeCall(accountId, "tester");
  CHECK(waitFor([&] { return historyCount("Outgoing") == outgoingBefore + 1; }));
  CHECK(waitFor([] { return CallModel::instance().rowCount() == 0; }));
  CHECK(waitFor([] { return historyCount("Missed") == 2; }));
  CHECK(incomingSignals == 0);
  CHECK(lastFinalStatus == 486);
  CHECK(historyCount("Incoming") == 2); // the two answered calls above
  sip.setDoNotDisturb(false);

  // --- instant message to ourselves lands in one conversation ---------------
  MessageModel messages;
  messages.setCurrentRemoteParty("tester");
  messages.sendMessage("hello");
  CHECK(waitFor([&] { return messages.rowCount() == 2; }));
  sip.sendTypingIndication(accountId, "tester", true);
  pump(200);

  // --- ICE: media negotiates candidates and comes up ------------------------
  sip.setAutoAnswer(true);
  account["ice"] = true;
  accounts.updateAccount(0, account);
  callService.makeCall(accountId, "tester");
  CHECK(waitFor([] { return allConfirmed(2); }));
  CHECK(waitFor([] { return iceRunning(callId(0)); }));
  callService.hangupCall(callId(0));
  CHECK(waitFor([] { return CallModel::instance().rowCount() == 0; }));

  // --- TURN: with direct media impossible, audio flows through the relay ----
  // Needs a TURN server, so it only runs when one is named, e.g. PJSIP's own
  // pjturn-srv:  MICROSIP_TEST_TURN=127.0.0.1:3478 (user/password 100/100)
  const QString turnServer = qEnvironmentVariable("MICROSIP_TEST_TURN");
  if (!turnServer.isEmpty()) {
    account["ice"] = false; // TURN alone must be enough to enable ICE
    account["turnServer"] = turnServer;
    account["turnUser"] = qEnvironmentVariable("MICROSIP_TEST_TURN_USER", "100");
    account["turnPass"] = qEnvironmentVariable("MICROSIP_TEST_TURN_PASS", "100");
    accounts.updateAccount(0, account);

    // Offer no host candidates, leaving the relay as the only path
    const pjsua_acc_id acc = sip.getAccount(accountId)->getId();
    pj_pool_t *pool = pjsua_pool_create("turn-test", 1000, 1000);
    pjsua_acc_config accCfg;
    CHECK(pjsua_acc_get_config(acc, pool, &accCfg) == PJ_SUCCESS);
    accCfg.ice_cfg_use = PJSUA_ICE_CONFIG_USE_CUSTOM;
    accCfg.ice_cfg.ice_max_host_cands = 0;
    CHECK(pjsua_acc_modify(acc, &accCfg) == PJ_SUCCESS);
    pj_pool_release(pool);

    callService.makeCall(accountId, "tester");
    CHECK(waitFor([] { return allConfirmed(2); }, 20000));
    const int relayed = callId(0);
    CHECK(waitFor([&] { return iceRunning(relayed); }, 20000));
    pjmedia_transport_info tp;
    const pjmedia_ice_transport_info *ice = iceInfo(relayed, tp);
    CHECK(ice->comp[0].lcand_type == PJ_ICE_CAND_TYPE_RELAYED);
    CHECK(waitFor([&] {
      pjsua_stream_stat stat;
      return pjsua_call_get_stream_stat(relayed, 0, &stat) == PJ_SUCCESS &&
             stat.rtcp.rx.pkt > 0;
    }));
    callService.hangupCall(relayed);
    CHECK(waitFor([] { return CallModel::instance().rowCount() == 0; }));
    std::puts("call_flow_test: TURN relay verified");
  }

  // --- every window loads against the populated models without QML errors --
  {
    qInstallMessageHandler(collectQmlWarnings);
    QQmlApplicationEngine engine;
    setupQml(engine);
    for (const char *file : {"main.qml", "SettingsWindow.qml",
                             "InCallWindow.qml", "MessageWindow.qml"}) {
      QQmlComponent component(&engine, QUrl(QString("qrc:/ui/%1").arg(file)));
      QObject *window = component.create();
      if (!window)
        std::fprintf(stderr, "%s\n", qPrintable(component.errorString()));
      CHECK(window);
      // Only the call and chat windows have these; a no-op elsewhere
      window->setProperty("remoteUri", "tester");
      window->setProperty("remoteParty", "tester");
      pump(300);
      delete window;
    }
    pump(100);
    qInstallMessageHandler(nullptr);
    for (const QString &warning : qmlWarnings)
      std::fprintf(stderr, "QML: %s\n", qPrintable(warning));
    CHECK(qmlWarnings.isEmpty());
  }

  // --- removing the account while a buddy is subscribed must not crash ------
  accounts.removeAccount(0);
  CHECK(accounts.rowCount() == 0);
  CHECK(!sip.getAccount(accountId));
  contacts.removeContact(0);
  CHECK(contacts.rowCount() == 0);
  pump(200);

  std::puts("call_flow_test: ALL OK");
  return 0; // normal exit also exercises the shutdown order
}
