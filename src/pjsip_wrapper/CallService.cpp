#include "CallService.h"
#include "../models/CallModel.h"
#include "SipService.h"
#include "SipUri.h"
#include "StorageService.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

using namespace pj;

MyCall::MyCall(Account &acc, int call_id) : Call(acc, call_id) {}

MyCall::~MyCall() {}

// PJSIP invokes these on its worker thread. Capture what we need here and hop
// to the GUI thread before touching CallService state or any model.
void MyCall::onCallState(pj::OnCallStateParam &) {
  CallInfo ci = getInfo();
  qDebug() << "*** Call State:" << ci.stateText.c_str();

  const int id = getId();
  const bool disconnected = ci.state == PJSIP_INV_STATE_DISCONNECTED;
  // pjsip spells this state "DISCONNCTD"; the UI matches on "DISCONNECTED"
  const QString state =
      disconnected ? "DISCONNECTED" : QString::fromStdString(ci.stateText);
  const int status = ci.lastStatusCode;
  const QString reason = QString::fromStdString(ci.lastReason);
  const QString remoteUri = QString::fromStdString(ci.remoteUri);
  const bool answered = ci.connectDuration.sec || ci.connectDuration.msec;
  const QString direction = (ci.role == PJSIP_ROLE_UAC) ? "Outgoing"
                            : answered                  ? "Incoming"
                                                        : "Missed";
  const int duration = ci.connectDuration.sec;
  const bool quiet = silent;

  QMetaObject::invokeMethod(
      &CallService::instance(),
      [=] {
        CallService &svc = CallService::instance();
        emit svc.callStateChanged(id, state, status, reason);
        if (disconnected) {
          if (!quiet)
            StorageService::instance().addCall(remoteUri, direction, duration);
          svc.removeCall(id);
        }
      },
      Qt::QueuedConnection);
}

void MyCall::onCallMediaState(pj::OnCallMediaStateParam &) {
  const int id = getId();
  QMetaObject::invokeMethod(
      &CallService::instance(),
      [id] { CallService::instance().handleMediaState(id); },
      Qt::QueuedConnection);
}

// Wires the call's audio to the sound device (honouring mute) and reports
// video state. Runs on the GUI thread.
void CallService::handleMediaState(int callId) {
  auto call = m_activeCalls.value(callId);
  if (!call)
    return;

  bool hasVideo = false;
  try {
    CallInfo ci = call->getInfo();
    AudDevManager &adm = Endpoint::instance().audDevManager();
    auto recorder = m_recorders.find(callId);
    for (unsigned i = 0; i < ci.media.size(); i++) {
      if (ci.media[i].type == PJMEDIA_TYPE_AUDIO && call->getMedia(i)) {
        AudioMedia *aud_med = (AudioMedia *)call->getMedia(i);
        aud_med->startTransmit(adm.getPlaybackDevMedia());
        if (m_mutedCalls.contains(callId))
          adm.getCaptureDevMedia().stopTransmit(*aud_med);
        else
          adm.getCaptureDevMedia().startTransmit(*aud_med);
        if (recorder != m_recorders.end()) {
          aud_med->startTransmit(*recorder->second);
          // A muted microphone is not recorded either
          if (m_mutedCalls.contains(callId))
            adm.getCaptureDevMedia().stopTransmit(*recorder->second);
          else
            adm.getCaptureDevMedia().startTransmit(*recorder->second);
        }
        // The far end is sending audio (early media): stop our ringback tone
        if (m_outgoingCalls.contains(callId) &&
            ci.media[i].status == PJSUA_CALL_MEDIA_ACTIVE)
          setRinging(callId, false);
      } else if (ci.media[i].type == PJMEDIA_TYPE_VIDEO &&
                 ci.media[i].status == PJSUA_CALL_MEDIA_ACTIVE) {
        hasVideo = true;
      }
    }
  } catch (Error &err) {
    qWarning() << "Error connecting audio:"
               << QString::fromStdString(err.info());
  }

  emit callMediaStateChanged(callId, hasVideo);
}

CallService &CallService::instance() {
  static CallService _instance;
  return _instance;
}

CallService::CallService(QObject *parent) : QObject(parent) {
  connect(this, &CallService::outgoingCall, [this](int id, QString uri) {
    m_lastCallId = id;
    m_lastRemoteUri = uri;
  });
  connect(this, &CallService::callStateChanged,
          [this](int id, QString state, int status, QString reason) {
            // An incoming call rings until answered; an outgoing one plays
            // the same tone as ringback once the far end is ringing
            if (state == "EARLY" && m_outgoingCalls.contains(id))
              setRinging(id, true);
            else if (state != "INCOMING" && state != "EARLY")
              setRinging(id, false);
            CallModel::instance().updateCallState(id, state, status, reason);
          });
  connect(this, &CallService::callMediaStateChanged, [](int id, bool hasVideo) {
    CallModel::instance().updateCallMedia(id, hasVideo);
  });
}

CallService::~CallService() {}

void CallService::makeCall(const QString &accountId,
                           const QString &destination, bool video) {
  // An IP address as host is a direct call: it bypasses the account's
  // registrar and proxy and needs no account at all. Anything else goes
  // through the account, a bare user getting its domain ("user@domain").
  // One call at a time: the one in progress has to be put on hold first
  if (CallModel::instance().activeCallId() >= 0) {
    emit callFailed(destination, "A call is in progress. Put it on hold first.");
    return;
  }
  QString rawDest = bareSipUri(destination);
  const bool ipCall = hasIpHost(rawDest);
  auto acc = ipCall ? SipService::instance().localAccount()
                    : SipService::instance().getAccount(accountId);
  if (!acc) {
    qWarning() << "No account to call" << destination << "with:" << accountId;
    emit callFailed(destination, ipCall ? "IP calls are unavailable"
                                        : "No active account");
    return;
  }
  if (!ipCall && !rawDest.contains('@'))
    rawDest += accountId.mid(accountId.indexOf('@'));
  const QString dest = "<sip:" + rawDest + ">";

  auto call = std::make_shared<MyCall>(*acc);
  CallOpParam prm(true);
  prm.opt.textCount = 0; // no real-time text line; PJSIP offers one by default
  prm.opt.audioCount = 1;
  prm.opt.videoCount = (video && videoAvailable()) ? 1 : 0;

  qInfo() << "Making call to:" << dest;

  try {
    call->makeCall(dest.toStdString(), prm);
    addCall(call);
    m_outgoingCalls.insert(call->getId());
    m_lastCallId = call->getId();
    m_lastRemoteUri = destination;
    CallModel::instance().addCall(m_lastCallId, destination);
    // In progress from this moment, not when PJSIP reports back: a second
    // call asked for straight away has to be refused
    CallModel::instance().updateCallState(m_lastCallId, "CALLING");
    emit outgoingCall(m_lastCallId, destination);
  } catch (Error &err) {
    qCritical() << "Error making call:" << QString::fromStdString(err.info());
    emit callFailed(destination, QString::fromStdString(err.reason));
  }
}

void CallService::answerCall(int callId) {
  if (m_activeCalls.contains(callId)) {
    // Answer with audio only; video is sent only on calls the user starts
    CallOpParam prm(true);
    prm.opt.textCount = 0; // no real-time text line; PJSIP offers one by default
    prm.opt.audioCount = 1;
    prm.opt.videoCount = 0;
    prm.statusCode = PJSIP_SC_OK;
    holdOthers(callId);
    try {
      m_activeCalls[callId]->answer(prm);
    } catch (Error &err) {
      qWarning() << "Error answering call:"
                 << QString::fromStdString(err.info());
    }
  }
}

void CallService::hangupCall(int callId) {
  if (m_activeCalls.contains(callId)) {
    CallOpParam prm;
    prm.statusCode = PJSIP_SC_DECLINE;
    try {
      m_activeCalls[callId]->hangup(prm);
    } catch (Error &err) {
      qWarning() << "Error hanging up call:"
                 << QString::fromStdString(err.info());
    }
  }
}

void CallService::addCall(std::shared_ptr<MyCall> call) {
  m_activeCalls[call->getId()] = call;
}

// The model row is added before the signal goes out: QML handlers run ahead
// of C++ slots, and the UI looks the call up in the model straight away.
void CallService::announceIncoming(std::shared_ptr<MyCall> call,
                                   const QString &remoteUri) {
  const int id = call->getId();
  addCall(call);
  m_lastCallId = id;
  m_lastRemoteUri = remoteUri;
  CallModel::instance().addCall(id, remoteUri);
  setRinging(id, true);
  emit incomingCall(id, remoteUri);
}

void CallService::removeCall(int callId) {
  m_activeCalls.remove(callId);
  m_conferenceParticipants.remove(callId);
  m_mutedCalls.remove(callId);
  m_heldCalls.remove(callId);
  m_outgoingCalls.remove(callId);
  m_recorders.erase(callId);
  setRinging(callId, false);
  CallModel::instance().removeCall(callId);
  // Queued: the call object above has to be gone before accounts are rebuilt
  QMetaObject::invokeMethod(
      &SipService::instance(),
      [] { SipService::instance().applyDeferredAccounts(); },
      Qt::QueuedConnection);
}

void CallService::toggleMute(int callId) {
  if (!m_activeCalls.contains(callId))
    return;
  if (!m_mutedCalls.remove(callId))
    m_mutedCalls.insert(callId);
  handleMediaState(callId);
}

// One conversation at a time: starting or answering a call holds the others,
// except those the user joined into a conference.
void CallService::holdOthers(int exceptCallId) {
  const QList<int> ids = m_activeCalls.keys();
  for (int id : ids) {
    if (id == exceptCallId || m_heldCalls.contains(id) ||
        m_conferenceParticipants.contains(id))
      continue;
    try {
      if (m_activeCalls[id]->getInfo().state == PJSIP_INV_STATE_CONFIRMED)
        holdCall(id);
    } catch (Error &) {
    }
  }
}

bool CallService::hasConnectedCall(int exceptCallId) const {
  for (auto it = m_activeCalls.cbegin(); it != m_activeCalls.cend(); ++it) {
    if (it.key() == exceptCallId)
      continue;
    try {
      if (it.value()->getInfo().state == PJSIP_INV_STATE_CONFIRMED)
        return true;
    } catch (Error &) {
    }
  }
  return false;
}

QUrl CallService::recordingsUrl() const {
  return QUrl::fromLocalFile(recordingsDir());
}

QString CallService::recordingsDir() const {
  QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
           "/recordings");
  dir.mkpath(".");
  return dir.absolutePath();
}

bool CallService::toggleRecording(int callId) {
  if (!m_activeCalls.contains(callId))
    return false;
  if (m_recorders.erase(callId)) // destroying the recorder closes the file
    return false;

  try {
    const QString file = QDir(recordingsDir()).filePath(
        QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") +
        QString("-call%1.wav").arg(callId));
    auto recorder = std::make_unique<AudioMediaRecorder>();
    recorder->createRecorder(file.toStdString());
    m_recorders[callId] = std::move(recorder);
    handleMediaState(callId);
    qInfo() << "Recording call" << callId << "to" << file;
    return true;
  } catch (Error &err) {
    qWarning() << "Error starting recording:"
               << QString::fromStdString(err.info());
    return false;
  }
}

// Plays a ring tone on the playback device while any incoming call is unanswered
void CallService::setRinging(int callId, bool ringing) {
  if (ringing)
    m_ringingCalls.insert(callId);
  else if (!m_ringingCalls.remove(callId))
    return;

  try {
    if (m_ringingCalls.isEmpty()) {
      if (m_ringTone)
        m_ringTone->stop();
      return;
    }
    if (!m_ringTone) {
      m_ringTone = std::make_unique<ToneGenerator>();
      m_ringTone->createToneGenerator();
      m_ringTone->startTransmit(
          Endpoint::instance().audDevManager().getPlaybackDevMedia());
    }
    if (!m_ringTone->isBusy()) {
      ToneDesc tone;
      tone.freq1 = 440;
      tone.freq2 = 480;
      tone.on_msec = 2000;
      tone.off_msec = 4000;
      ToneDescVector tones;
      tones.push_back(tone);
      m_ringTone->play(tones, true);
    }
  } catch (Error &err) {
    qWarning() << "Error playing ring tone:"
               << QString::fromStdString(err.info());
  }
}

void CallService::holdCall(int callId) {
  if (m_activeCalls.contains(callId)) {
    try {
      CallOpParam prm(true);
      prm.opt.textCount = 0; // no real-time text line; PJSIP offers one by default
      m_activeCalls[callId]->setHold(prm);
      m_heldCalls.insert(callId);
      CallModel::instance().updateCallHold(callId, true);
    } catch (Error &err) {
      qWarning() << "Error holding call:" << QString::fromStdString(err.info());
    }
  }
}

void CallService::resumeCall(int callId) {
  if (m_activeCalls.contains(callId)) {
    try {
      CallOpParam prm(true);
      prm.opt.textCount = 0; // no real-time text line; PJSIP offers one by default
      prm.opt.flag |= PJSUA_CALL_UNHOLD;
      m_activeCalls[callId]->reinvite(prm);
      m_heldCalls.remove(callId);
      CallModel::instance().updateCallHold(callId, false);
    } catch (Error &err) {
      qWarning() << "Error resuming call:"
                 << QString::fromStdString(err.info());
    }
  }
}

void CallService::transferCall(int callId, const QString &destination) {
  if (m_activeCalls.contains(callId)) {
    try {
      // A bare extension is on the same server as the call: our own domain,
      // or the far end's host for a direct IP call
      QString target = bareSipUri(destination);
      if (!target.contains('@') && !hasIpHost(target)) {
        const CallInfo ci = m_activeCalls[callId]->getInfo();
        const QString local = bareSipUri(QString::fromStdString(ci.localUri));
        const QString remote = bareSipUri(QString::fromStdString(ci.remoteUri));
        target += "@" + (local.contains('@') ? local : remote).section('@', -1);
      }
      CallOpParam prm;
      m_activeCalls[callId]->xfer(("sip:" + target).toStdString(), prm);
    } catch (Error &err) {
      qWarning() << "Error transferring call:"
                 << QString::fromStdString(err.info());
    }
  }
}

void CallService::attendedTransferCall(int callId, int destinationCallId) {
  if (m_activeCalls.contains(callId) &&
      m_activeCalls.contains(destinationCallId)) {
    try {
      CallOpParam prm;
      m_activeCalls[callId]->xferReplaces(*m_activeCalls[destinationCallId],
                                          prm);
    } catch (Error &err) {
      qWarning() << "Error in attended transfer:"
                 << QString::fromStdString(err.info());
    }
  }
}

void CallService::sendDtmf(int callId, const QString &digits) {
  if (m_activeCalls.contains(callId)) {
    try {
      m_activeCalls[callId]->dialDtmf(digits.toStdString());
    } catch (Error &err) {
      qWarning() << "Error sending DTMF:" << QString::fromStdString(err.info());
    }
  }
}

void CallService::addToConference(int callId) {
  if (m_activeCalls.contains(callId)) {
    m_conferenceParticipants.insert(callId);
    CallModel::instance().updateCallConference(callId, true);
    if (m_heldCalls.contains(callId))
      resumeCall(callId);

    // Dynamic bridging: connect this call to all other conference participants
    auto call = m_activeCalls[callId];
    try {
      for (int otherId : m_conferenceParticipants) {
        if (otherId == callId)
          continue;
        auto otherCall = m_activeCalls[otherId];

        // Connect audio media
        for (unsigned i = 0; i < call->getInfo().media.size(); i++) {
          if (call->getInfo().media[i].type == PJMEDIA_TYPE_AUDIO) {
            AudioMedia *aud = (AudioMedia *)call->getMedia(i);
            for (unsigned j = 0; j < otherCall->getInfo().media.size(); j++) {
              if (otherCall->getInfo().media[j].type == PJMEDIA_TYPE_AUDIO) {
                AudioMedia *otherAud = (AudioMedia *)otherCall->getMedia(j);
                aud->startTransmit(*otherAud);
                otherAud->startTransmit(*aud);
              }
            }
          }
        }
      }
    } catch (Error &err) {
      qWarning() << "Error bridging conference:"
                 << QString::fromStdString(err.info());
    }
  }
}

void CallService::removeFromConference(int callId) {
  if (m_conferenceParticipants.contains(callId)) {
    m_conferenceParticipants.remove(callId);
    CallModel::instance().updateCallConference(callId, false);

    auto call = m_activeCalls[callId];
    try {
      for (int otherId : m_activeCalls.keys()) {
        if (otherId == callId)
          continue;
        auto otherCall = m_activeCalls[otherId];

        // Disconnect audio media
        for (unsigned i = 0; i < call->getInfo().media.size(); i++) {
          if (call->getInfo().media[i].type == PJMEDIA_TYPE_AUDIO) {
            AudioMedia *aud = (AudioMedia *)call->getMedia(i);
            for (unsigned j = 0; j < otherCall->getInfo().media.size(); j++) {
              if (otherCall->getInfo().media[j].type == PJMEDIA_TYPE_AUDIO) {
                AudioMedia *otherAud = (AudioMedia *)otherCall->getMedia(j);
                aud->stopTransmit(*otherAud);
                otherAud->stopTransmit(*aud);
              }
            }
          }
        }
      }
    } catch (Error &err) {
      qWarning() << "Error unbridging conference:"
                 << QString::fromStdString(err.info());
    }
  }
}

bool CallService::isConferenceActive() const {
  return m_conferenceParticipants.size() > 1;
}
