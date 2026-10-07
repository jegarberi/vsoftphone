#include "CallService.h"
#include "../models/CallModel.h"
#include "SipService.h"
#include "SipUri.h"
#include "StorageService.h"
#include <QDebug>

using namespace pj;

MyCall::MyCall(Account &acc, int call_id) : Call(acc, call_id) {}

MyCall::~MyCall() {}

// PJSIP invokes these on its worker thread. Capture what we need here and hop
// to the GUI thread before touching CallService state or any model.
void MyCall::onCallState(pj::OnCallStateParam &prm) {
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
  const QString direction =
      (ci.role == PJSIP_ROLE_UAC) ? "Outgoing" : "Incoming";
  const int duration = ci.totalDuration.sec;

  QMetaObject::invokeMethod(
      &CallService::instance(),
      [=] {
        CallService &svc = CallService::instance();
        emit svc.callStateChanged(id, state, status, reason);
        if (disconnected) {
          StorageService::instance().addCall(remoteUri, direction, duration);
          svc.removeCall(id);
        }
      },
      Qt::QueuedConnection);
}

void MyCall::onCallMediaState(pj::OnCallMediaStateParam &prm) {
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
    for (unsigned i = 0; i < ci.media.size(); i++) {
      if (ci.media[i].type == PJMEDIA_TYPE_AUDIO && call->getMedia(i)) {
        AudioMedia *aud_med = (AudioMedia *)call->getMedia(i);
        aud_med->startTransmit(adm.getPlaybackDevMedia());
        if (m_mutedCalls.contains(callId))
          adm.getCaptureDevMedia().stopTransmit(*aud_med);
        else
          adm.getCaptureDevMedia().startTransmit(*aud_med);
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
  connect(this, &CallService::incomingCall, [this](int id, QString uri) {
    m_lastCallId = id;
    m_lastRemoteUri = uri;
    CallModel::instance().addCall(id, uri);
  });
  connect(this, &CallService::outgoingCall, [this](int id, QString uri) {
    m_lastCallId = id;
    m_lastRemoteUri = uri;
  });
  connect(this, &CallService::callStateChanged,
          [](int id, QString state, int status, QString reason) {
            CallModel::instance().updateCallState(id, state, status, reason);
          });
  connect(this, &CallService::callMediaStateChanged, [](int id, bool hasVideo) {
    CallModel::instance().updateCallMedia(id, hasVideo);
  });
}

CallService::~CallService() {}

void CallService::makeCall(const QString &accountId,
                           const QString &destination) {
  auto acc = SipService::instance().getAccount(accountId);
  if (!acc) {
    qWarning() << "Account not found:" << accountId;
    return;
  }

  auto call = std::make_shared<MyCall>(*acc);
  CallOpParam prm(true);
  prm.opt.audioCount = 1;
  prm.opt.videoCount = 0; // Disable video by default to keep SDP small

  // Bare user gets the account's domain (accountId is "user@domain")
  QString rawDest = bareSipUri(destination);
  if (!rawDest.contains('@'))
    rawDest += accountId.mid(accountId.indexOf('@'));
  const QString dest = "<sip:" + rawDest + ">";

  qInfo() << "Making call to:" << dest;

  try {
    call->makeCall(dest.toStdString(), prm);
    addCall(call);
    m_lastCallId = call->getId();
    m_lastRemoteUri = destination;
    CallModel::instance().addCall(m_lastCallId, destination);
    emit outgoingCall(m_lastCallId, destination);
  } catch (Error &err) {
    qCritical() << "Error making call:" << QString::fromStdString(err.info());
  }
}

void CallService::answerCall(int callId) {
  if (m_activeCalls.contains(callId)) {
    CallOpParam prm;
    prm.statusCode = PJSIP_SC_OK;
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

void CallService::removeCall(int callId) {
  m_activeCalls.remove(callId);
  m_conferenceParticipants.remove(callId);
  m_mutedCalls.remove(callId);
  CallModel::instance().removeCall(callId);
}

void CallService::toggleMute(int callId) {
  if (!m_activeCalls.contains(callId))
    return;
  if (!m_mutedCalls.remove(callId))
    m_mutedCalls.insert(callId);
  handleMediaState(callId);
}

void CallService::holdCall(int callId) {
  if (m_activeCalls.contains(callId)) {
    try {
      CallOpParam prm(true);
      m_activeCalls[callId]->setHold(prm);
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
      prm.opt.flag |= PJSUA_CALL_UNHOLD;
      m_activeCalls[callId]->reinvite(prm);
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
      CallOpParam prm;
      m_activeCalls[callId]->xfer(destination.toStdString(), prm);
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
