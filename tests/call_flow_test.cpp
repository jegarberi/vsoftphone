// End-to-end check of the service layer without a SIP server: the endpoint
// calls and messages its own UDP port, with the null audio device.
//
// Run: ctest --preset conan-release   (or build/Release/call_flow_test)

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <pjsip_simple.h>
#include <pjsip_ua.h>
#include <pjsua-lib/pjsua.h>
#include <string>

#include "QmlSetup.h"
#include "models/AccountModel.h"
#include "models/CallModel.h"
#include "models/ContactsModel.h"
#include "models/HistoryModel.h"
#include "models/MessageModel.h"
#include "pjsip_wrapper/BlfService.h"
#include "pjsip_wrapper/CallService.h"
#include "pjsip_wrapper/Keyring.h"
#include "pjsip_wrapper/SipService.h"
#include "pjsip_wrapper/SipUri.h"
#include "pjsip_wrapper/StorageService.h"
#include "ui/Theme.h"

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

// What a BroadWorks server sends for a BLF list: a resource list (RLMI) and
// one dialog-info document per monitored user, as multipart/related
static const QByteArray kBlfNotify =
    "--b1\r\nContent-Type: application/rlmi+xml;charset=\"UTF-8\"\r\n"
    "Content-ID: <list>\r\n\r\n"
    "<?xml version=\"1.0\"?><list xmlns=\"urn:ietf:params:xml:ns:rlmi\" "
    "uri=\"sip:myblf@as.example.com\" version=\"1\" fullState=\"true\">"
    "<resource uri=\"sip:2001@as.example.com\"><name>Alice A</name>"
    "<instance id=\"a\" state=\"active\" cid=\"c1\"/></resource>"
    "<resource uri=\"sip:2002@as.example.com\"><name>Bob B</name>"
    "<instance id=\"b\" state=\"active\" cid=\"c2\"/></resource>"
    "<resource uri=\"sip:2003@as.example.com\">"
    "<instance id=\"c\" state=\"active\" cid=\"c3\"/></resource></list>\r\n"
    "--b1\r\nContent-Type: application/dialog-info+xml\r\n"
    "Content-ID: <c1>\r\n\r\n"
    "<dialog-info xmlns=\"urn:ietf:params:xml:ns:dialog-info\" version=\"0\" "
    "state=\"full\" entity=\"sip:2001@as.example.com\"><dialog id=\"x\" "
    "direction=\"recipient\"><state>confirmed</state></dialog></dialog-info>"
    "\r\n--b1\r\nContent-Type: application/dialog-info+xml\r\n\r\n"
    "<dialog-info xmlns=\"urn:ietf:params:xml:ns:dialog-info\" version=\"0\" "
    "state=\"full\" entity=\"sip:2002@as.example.com\"><dialog id=\"y\">"
    "<state>early</state></dialog></dialog-info>\r\n"
    "--b1\r\nContent-Type: application/dialog-info+xml\r\n\r\n"
    "<dialog-info xmlns=\"urn:ietf:params:xml:ns:dialog-info\" version=\"0\" "
    "state=\"full\" entity=\"sip:2003@as.example.com\"/>\r\n--b1--\r\n";

// A stand-in for that server, inside our own endpoint: it accepts a SUBSCRIBE
// to the list "blflist" and answers with the NOTIFY above. This is what the
// real exchange looks like on the wire, short of authentication.
static std::string blfServerContact;

static pj_bool_t blfServerOnRxRequest(pjsip_rx_data *rdata) {
  pjsip_msg *msg = rdata->msg_info.msg;
  if (pjsip_method_cmp(&msg->line.req.method, pjsip_get_subscribe_method()) != 0 ||
      rdata->msg_info.to->tag.slen != 0) // refreshes belong to the dialog
    return PJ_FALSE;
  auto *uri = static_cast<pjsip_sip_uri *>(pjsip_uri_get_uri(msg->line.req.uri));
  if (pj_strcmp2(&uri->user, "blflist") != 0)
    return PJ_FALSE;

  pj_str_t contact = pj_str(const_cast<char *>(blfServerContact.c_str()));
  pjsip_dialog *dlg = nullptr;
  CHECK(pjsip_dlg_create_uas_and_inc_lock(pjsip_ua_instance(), rdata, &contact,
                                          &dlg) == PJ_SUCCESS);
  pjsip_evsub_user callbacks;
  pj_bzero(&callbacks, sizeof(callbacks));
  callbacks.on_rx_refresh = [](pjsip_evsub *, pjsip_rx_data *, int *st_code,
                               pj_str_t **, pjsip_hdr *, pjsip_msg_body **) {
    *st_code = 200; // refreshed, or ended with Expires: 0
  };
  pjsip_evsub *sub = nullptr;
  CHECK(pjsip_evsub_create_uas(dlg, &callbacks, rdata, 0, &sub) == PJ_SUCCESS);
  CHECK(pjsip_evsub_accept(sub, rdata, 200, nullptr) == PJ_SUCCESS);

  pjsip_tx_data *tdata = nullptr;
  CHECK(pjsip_evsub_notify(sub, PJSIP_EVSUB_STATE_ACTIVE, nullptr, nullptr,
                           &tdata) == PJ_SUCCESS);
  pj_str_t type = pj_str((char *)"multipart");
  pj_str_t subtype = pj_str((char *)"related");
  pj_str_t text = {const_cast<char *>(kBlfNotify.constData()),
                   pj_ssize_t(kBlfNotify.size())};
  tdata->msg->body = pjsip_msg_body_create(tdata->pool, &type, &subtype, &text);
  auto *boundary = PJ_POOL_ZALLOC_T(tdata->pool, pjsip_param);
  boundary->name = pj_str((char *)"boundary");
  boundary->value = pj_str((char *)"b1");
  pj_list_push_back(&tdata->msg->body->content_type.param, boundary);
  CHECK(pjsip_evsub_send_request(sub, tdata) == PJ_SUCCESS);
  pjsip_dlg_dec_lock(dlg);
  return PJ_TRUE;
}

static pjsip_module blfServerModule = {
    nullptr, nullptr, {(char *)"mod-test-blf-server", 19}, -1,
    PJSIP_MOD_PRIORITY_APPLICATION - 1, // ahead of PJSUA, which answers 489
    nullptr, nullptr, nullptr, nullptr, &blfServerOnRxRequest, nullptr,
    nullptr, nullptr, nullptr};

// Whether the account advertises the server-seen address in SDP; one of the
// settings PJSIP cannot change under a live call
static bool sdpNatRewrite(const QString &accountId) {
  pj_pool_t *pool = pjsua_pool_create("cfg", 1000, 1000);
  pjsua_acc_config cfg;
  CHECK(pjsua_acc_get_config(
            SipService::instance().getAccount(accountId)->getId(), pool,
            &cfg) == PJ_SUCCESS);
  const bool rewrite = cfg.allow_sdp_nat_rewrite;
  pj_pool_release(pool);
  return rewrite;
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
  QSettings("Antigravity", "MicrosipClone").clear(); // Theme's file, test copy

  // --- address classification ------------------------------------------------
  CHECK(hasIpHost("10.0.0.5") && hasIpHost("bob@10.0.0.5:5080"));
  CHECK(hasIpHost("[fe80::1]:5060"));
  CHECK(!hasIpHost("1001") && !hasIpHost("bob@pbx.example.com"));
  CHECK(!hasIpHost("1001@pbx.example.com:5060"));

  // --- password store round trip, only on request: it uses the real store ---
  // MICROSIP_TEST_KEYRING=1 build/Release/call_flow_test
  if (qEnvironmentVariableIsSet("MICROSIP_TEST_KEYRING")) {
    const QString key = "call_flow_test/selftest";
    bool found = true;
    CHECK(Keyring::available());
    Keyring::clear(key);
    Keyring::lookup(key, &found);
    CHECK(!found);
    CHECK(Keyring::store(key, "s3cret \xc3\xb1"));
    CHECK(Keyring::lookup(key, &found) == QString::fromUtf8("s3cret \xc3\xb1"));
    CHECK(found);
    Keyring::clear(key);
    Keyring::lookup(key, &found);
    CHECK(!found);
    std::puts("call_flow_test: password store verified");
  }

  // --- BroadWorks BLF NOTIFY: resource list of dialog-info documents ---------
  {
    const QList<BlfLamp> lamps = BlfService::parseNotify("b1", kBlfNotify);
    CHECK(lamps.size() == 3);
    CHECK(lamps[0].uri == "2001@as.example.com" && lamps[0].state == "Busy");
    CHECK(lamps[0].name == "Alice A");
    CHECK(lamps[1].uri == "2002@as.example.com" && lamps[1].state == "Ringing");
    CHECK(lamps[2].state == "Idle" && lamps[2].name.isEmpty());

    // A single-user subscription gets a bare dialog-info body
    const QList<BlfLamp> single = BlfService::parseNotify(
        "", "<dialog-info entity=\"sip:2009@x.org\" state=\"full\"><dialog "
            "id=\"z\"><state>terminated</state></dialog></dialog-info>");
    CHECK(single.size() == 1 && single[0].state == "Idle");
  }

  CHECK(StorageService::instance().init());
  SipService &sip = SipService::instance();
  sip.init();
  CHECK(sip.endpoint());
  sip.endpoint()->audDevManager().setNullDev();

  pjsua_transport_info ti;
  CHECK(pjsua_transport_get_info(0, &ti) == PJ_SUCCESS);
  const QString domain = QString("127.0.0.1:%1").arg(ti.local_name.port);
  blfServerContact = QString("<sip:blflist@%1>").arg(domain).toStdString();
  CHECK(pjsip_endpt_register_module(pjsua_get_pjsip_endpt(),
                                    &blfServerModule) == PJ_SUCCESS);
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
  // The contact lives on a port nobody answers: subscribing to ourselves can
  // deadlock PJSIP when the account is then deleted mid-SUBSCRIBE.
  contacts.addContact("Alice", "2001@127.0.0.1:9", "Team");
  CHECK(contacts.rowCount() == 1);
  CHECK(contacts.data(contacts.index(0), ContactsModel::GroupRole) == "Team");

  // --- busy lamps: a stored contact shows its lamp, unknown users get a row --
  BlfService &blf = BlfService::instance();
  emit blf.lampChanged(accountId, "2001@127.0.0.1:9", "Alice A", "Busy");
  emit blf.lampChanged(accountId, "2002@as.example.com", "Bob B", "Ringing");
  CHECK(contacts.rowCount() == 2);
  CHECK(contacts.data(contacts.index(0), ContactsModel::StatusRole) == "Busy");
  CHECK(contacts.data(contacts.index(1), ContactsModel::NameRole) == "Bob B");
  CHECK(contacts.data(contacts.index(1), ContactsModel::GroupRole) == "BLF");
  CHECK(contacts.data(contacts.index(1), ContactsModel::StatusRole) == "Ringing");
  contacts.removeContact(1); // a row from the server cannot be removed here
  CHECK(contacts.rowCount() == 2);
  // Lamps of another account's subscription are left alone; ours go away
  emit blf.lampsCleared("someone@else");
  CHECK(contacts.rowCount() == 2);
  emit blf.lampsCleared(accountId);
  CHECK(contacts.rowCount() == 1);
  CHECK(contacts.data(contacts.index(0), ContactsModel::StatusRole) ==
        "Offline");

  // --- BLF subscription goes out and a refusal is survived ------------------
  // Nothing here serves the dialog event, so the SUBSCRIBE is rejected.
  account["blfUri"] = "myblf";
  accounts.updateAccount(0, account);
  pump(500);

  // --- BLF end to end: subscribe to a list, receive its NOTIFY ----------------
  account["blfUri"] = "blflist";
  accounts.updateAccount(0, account);
  auto lampOf = [&](const QString &name) {
    for (int i = 0; i < contacts.rowCount(); ++i) {
      if (contacts.data(contacts.index(i), ContactsModel::NameRole) == name)
        return contacts.data(contacts.index(i), ContactsModel::StatusRole)
            .toString();
    }
    return QString();
  };
  CHECK(waitFor([&] { return contacts.rowCount() == 4; })); // Alice + 3 lamps
  CHECK(lampOf("Alice A") == "Busy");
  CHECK(lampOf("Bob B") == "Ringing");
  CHECK(lampOf("2003") == "Idle");
  account["blfUri"] = ""; // unsubscribing takes the lamps away again
  accounts.updateAccount(0, account);
  CHECK(contacts.rowCount() == 1);

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

  // An account in a call cannot be removed or switched off, and saving it
  // again must leave the call usable (everything below still runs on it)
  int busyRefusals = 0;
  QObject::connect(&accounts, &AccountModel::accountBusy, &app,
                   [&] { ++busyRefusals; });
  accounts.removeAccount(0);
  accounts.toggleActive(0);
  CHECK(busyRefusals == 2);
  CHECK(accounts.rowCount() == 1 && sip.getAccount(accountId));
  accounts.updateAccount(0, account);
  sip.applyNetworkSettings(); // nothing changed: must not touch the accounts
  CHECK(busyRefusals == 2 && allConfirmed(2));

  // A setting PJSIP cannot change under a live call waits for the call to end
  sip.setMediaRport(true);
  sip.applyNetworkSettings();
  CHECK(!sdpNatRewrite(accountId));

  CHECK(waitFor([&] { return micReaches(outgoing); }));
  // Since PJSIP 2.15 the conference bridge applies connections on its own
  // thread, so a change shows up a moment later
  callService.toggleMute(outgoing);
  CHECK(waitFor([&] { return !micReaches(outgoing); }));

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
  CHECK(waitFor([&] { return micReaches(outgoing); }));

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
  CHECK(waitFor([&] { return sdpNatRewrite(accountId); })); // applied now
  sip.setMediaRport(false);
  sip.applyNetworkSettings();
  CHECK(!sdpNatRewrite(accountId));

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

  // --- a message that cannot be delivered is shown as failed ---------------
  MessageModel undeliverable;
  undeliverable.setCurrentRemoteParty("nobody@invalid.invalid");
  undeliverable.sendMessage("lost");
  CHECK(waitFor([&] {
    return undeliverable.rowCount() == 1 &&
           undeliverable.data(undeliverable.index(0),
                              MessageModel::DirectionRole) == "Failed";
  }, 40000));
  // "tester@127.0.0.1" (incoming) is folded into the conversation "tester"
  CHECK(StorageService::instance().conversations() ==
        QStringList({"nobody@invalid.invalid", "tester"}));

  // --- global network settings reach PJSIP ----------------------------------
  auto accountConfig = [&](const std::function<void(const pjsua_acc_config &)>
                               &check) {
    pj_pool_t *pool = pjsua_pool_create("cfg", 1000, 1000);
    pjsua_acc_config cfg;
    CHECK(pjsua_acc_get_config(sip.getAccount(accountId)->getId(), pool,
                               &cfg) == PJ_SUCCESS);
    check(cfg);
    pj_pool_release(pool);
  };
  sip.setRport(false);
  CHECK(pjsip_cfg()->endpt.disable_rport == PJ_TRUE);
  sip.setRport(true);
  CHECK(pjsip_cfg()->endpt.disable_rport == PJ_FALSE);

  accountConfig([](const pjsua_acc_config &cfg) {
    CHECK(!cfg.allow_sdp_nat_rewrite);
    CHECK(cfg.sip_stun_use == PJSUA_STUN_USE_DISABLED); // account has it off
  });
  sip.setMediaRport(true);
  sip.setStunServer("127.0.0.1:9"); // the global default, nothing listens
  account["enableStun"] = true;     // account asks for STUN, names no server
  accounts.updateAccount(0, account);
  sip.applyNetworkSettings();
  accountConfig([](const pjsua_acc_config &cfg) {
    CHECK(cfg.allow_sdp_nat_rewrite);
    CHECK(cfg.sip_stun_use == PJSUA_STUN_USE_DEFAULT);
  });
  sip.setStunEnabled(false); // no global server and none of its own: off again
  sip.setMediaRport(false);
  sip.applyNetworkSettings();
  accountConfig([](const pjsua_acc_config &cfg) {
    CHECK(!cfg.allow_sdp_nat_rewrite);
    CHECK(cfg.sip_stun_use == PJSUA_STUN_USE_DISABLED);
  });
  account["enableStun"] = false;

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
    // The account names no TURN server, so it takes the global one, and
    // TURN alone must be enough to enable ICE
    account["ice"] = false;
    accounts.updateAccount(0, account);
    sip.setTurnServer(turnServer);
    sip.setTurnUser(qEnvironmentVariable("MICROSIP_TEST_TURN_USER", "100"));
    sip.setTurnPassword(qEnvironmentVariable("MICROSIP_TEST_TURN_PASS", "100"));
    sip.setTurnEnabled(true);
    sip.applyNetworkSettings();

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
    sip.setTurnEnabled(false);
    sip.applyNetworkSettings();
    std::puts("call_flow_test: TURN relay verified");
  }

  // --- account deleted while a SUBSCRIBE to it is in flight ------------------
  // A contact on our own endpoint makes us both watcher and presentity, so
  // every account rebuild races an incoming SUBSCRIBE against the deletion.
  contacts.addContact("Self", "tester");
  for (int i = 0; i < 15; ++i) {
    accounts.toggleActive(0); // deletes the account
    accounts.toggleActive(0); // and creates it again
    pump(30);
  }
  accounts.updateAccount(0, account); // modified in place: no deletion

  // --- every window loads against the populated models without QML errors --
  {
    qInstallMessageHandler(collectQmlWarnings);
    QQmlApplicationEngine engine;
    setupQml(engine);
    // MICROSIP_TEST_THEME=<name> picks the theme the screenshots are taken in
    if (qEnvironmentVariableIsSet("MICROSIP_TEST_THEME"))
      Theme::instance().setCurrent(qEnvironmentVariable("MICROSIP_TEST_THEME"));
    for (const char *file :
         {"main.qml", "SettingsWindow.qml", "InCallWindow.qml",
          "IncomingCallToast.qml"}) {
      QQmlComponent component(&engine, QUrl(QString("qrc:/ui/%1").arg(file)));
      QObject *window = component.create();
      if (!window)
        std::fprintf(stderr, "%s\n", qPrintable(component.errorString()));
      CHECK(window);
      // Only the call and chat windows have these; a no-op elsewhere
      window->setProperty("remoteUri", "tester");
      window->setProperty("remoteParty", "tester");
      if (QString(file) == "SettingsWindow.qml")
        CHECK(QMetaObject::invokeMethod(window, "applySettings")); // Apply button

      // Digits, * and # typed anywhere in the main window go to the dial pad
      if (QObject *display = window->findChild<QObject *>("dialDisplay")) {
        QObject *tabs = window->findChild<QObject *>("tabs");
        tabs->setProperty("currentIndex", 2); // typing from the History page
        pump(100);
        for (const char *key : {"5", "*", "#", "0"}) {
          QKeyEvent press(QEvent::KeyPress, 0, Qt::NoModifier, key);
          QCoreApplication::sendEvent(window, &press);
        }
        CHECK(display->property("text") == "5*#0");
        CHECK(tabs->property("currentIndex") == 0);
        QKeyEvent backspace(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &backspace);
        CHECK(display->property("text") == "5*#");
        display->setProperty("text", "");

        // An incoming call raises a toast; answering from it connects the
        // call and opens its window next to the caller's
        sip.setAutoAnswer(false);
        callService.makeCall(accountId, "tester");
        CHECK(waitFor([&] {
          return window->findChild<QObject *>("incomingToast") != nullptr;
        }));
        CHECK(window->findChildren<QObject *>("callWindow").size() == 1);
        CHECK(QMetaObject::invokeMethod(
            window->findChild<QObject *>("incomingToast"), "answer"));
        CHECK(waitFor([] { return allConfirmed(2); }));
        CHECK(waitFor([&] {
          return window->findChildren<QObject *>("callWindow").size() == 2 &&
                 !window->findChild<QObject *>("incomingToast");
        }));
        callService.hangupCall(callId(0));
        CHECK(waitFor([] { return CallModel::instance().rowCount() == 0; }));
        // Both call windows close themselves when their call ends
        CHECK(waitFor([&] {
          return window->findChildren<QObject *>("callWindow").isEmpty();
        }));
        sip.setAutoAnswer(true);
      }

      // MICROSIP_TEST_SCREENSHOTS=<dir> QT_QUICK_BACKEND=software saves a
      // picture of every window, and of each tab of the main one
      const QString shots = qEnvironmentVariable("MICROSIP_TEST_SCREENSHOTS");
      auto *quickWindow = qobject_cast<QQuickWindow *>(window);
      if (!shots.isEmpty() && quickWindow) {
        const QString base = shots + "/" + QString(file).section('.', 0, 0);
        if (QObject *tabs = window->findChild<QObject *>("tabs")) {
          for (int tab = 0; tab < 5; ++tab) {
            tabs->setProperty("currentIndex", tab);
            pump(250);
            quickWindow->grabWindow().save(QString("%1-%2.png").arg(base).arg(tab));
          }
        } else {
          window->setProperty("visible", true);
          window->setProperty("callState", "CONFIRMED");
          pump(250);
          quickWindow->grabWindow().save(base + ".png");
        }
      }
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
  CHECK(contacts.rowCount() == 2); // Self and Alice
  contacts.removeContact(0);
  CHECK(contacts.rowCount() == 1);
  pump(200);

  // --- direct IP call with no SIP account at all ---------------------------
  CHECK(accounts.defaultAccountId().isEmpty());
  int failures = 0;
  QObject::connect(&callService, &CallService::callFailed, &app,
                   [&] { ++failures; });
  callService.makeCall("", "1001"); // not an address and no account: refused
  CHECK(failures == 1);
  callService.makeCall("", "sip:" + domain); // our own "127.0.0.1:port"
  CHECK(waitFor([] { return allConfirmed(2); }));
  CHECK(failures == 1);
  callService.hangupCall(callId(0));
  CHECK(waitFor([] { return CallModel::instance().rowCount() == 0; }));

  // --- direct IP calls can be turned off: 403, no ringing, no history --------
  {
    const int ringsBefore = incomingSignals;
    const int missedBefore = historyCount("Missed");
    sip.setAcceptIpCalls(false);
    lastFinalStatus = 0;
    callService.makeCall("", "sip:" + domain);
    CHECK(waitFor([&] { return lastFinalStatus == 403; }));
    CHECK(waitFor([] { return CallModel::instance().rowCount() == 0; }));
    pump(200);
    CHECK(incomingSignals == ringsBefore);
    CHECK(historyCount("Missed") == missedBefore);
    sip.setAcceptIpCalls(true);
  }

  // --- starting a second call puts the first on hold ------------------------
  sip.setAutoAnswer(true);
  callService.makeCall("", "sip:" + domain);
  CHECK(waitFor([] { return allConfirmed(2); }));
  CallModel &calls = CallModel::instance();
  CHECK(!calls.data(calls.index(0), CallModel::IsHeldRole).toBool());
  callService.makeCall("", "sip:" + domain);
  CHECK(calls.data(calls.index(0), CallModel::IsHeldRole).toBool());
  CHECK(waitFor([] { return CallModel::instance().rowCount() == 4; }));
  pump(500);
  // Auto answer leaves the new call ringing while another is connected
  CHECK(callState(3) != "CONFIRMED");
  while (calls.rowCount() > 0) {
    const int rows = calls.rowCount();
    callService.hangupCall(callId(0));
    CHECK(waitFor([&] { return calls.rowCount() < rows; }));
  }

  // --- IPv6 call, where the host has IPv6 -----------------------------------
  pjsua_transport_id transports[8];
  unsigned transportCount = 8;
  pjsua_enum_transports(transports, &transportCount);
  for (unsigned i = 0; i < transportCount; ++i) {
    pjsua_transport_info info;
    if (pjsua_transport_get_info(transports[i], &info) != PJ_SUCCESS ||
        info.type != PJSIP_TRANSPORT_UDP6)
      continue;
    callService.makeCall("", QString("[::1]:%1").arg(info.local_name.port));
    CHECK(waitFor([] { return allConfirmed(2); }));
    callService.hangupCall(callId(0));
    CHECK(waitFor([] { return CallModel::instance().rowCount() == 0; }));
    std::puts("call_flow_test: IPv6 call verified");
  }

  std::puts("call_flow_test: ALL OK");
  return 0; // normal exit also exercises the shutdown order
}
