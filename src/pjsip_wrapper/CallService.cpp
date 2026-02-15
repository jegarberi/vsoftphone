#include "CallService.h"
#include "../models/CallModel.h"
#include "SipService.h"
#include "StorageService.h"
#include <QDebug>

using namespace pj;

MyCall::MyCall(Account &acc, int call_id) : Call(acc, call_id) {}

MyCall::~MyCall() {}

void MyCall::onCallState(pj::OnCallStateParam &prm) {
  CallInfo ci = getInfo();
  qDebug() << "*** Call State:" << ci.stateText.c_str();

  // Explicitly emit signals from CallService instance
  CallService::instance().callStateChanged(
      getId(), QString::fromStdString(ci.stateText));

  if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
    QString direction = (ci.role == PJSIP_ROLE_UAC) ? "Outgoing" : "Incoming";
    StorageService::instance().addCall(QString::fromStdString(ci.remoteUri),
                                       direction, ci.totalDuration.sec);
    CallService::instance().removeCall(getId());
  }
}

void MyCall::onCallMediaState(pj::OnCallMediaStateParam &prm) {
  CallInfo ci = getInfo();
  bool hasVideo = false;

  for (unsigned i = 0; i < ci.media.size(); i++) {
    if (ci.media[i].type == PJMEDIA_TYPE_AUDIO && getMedia(i)) {
      AudioMedia *aud_med = (AudioMedia *)getMedia(i);
      try {
        Endpoint::instance()
            .audDevManager()
            .getPlaybackDevMedia()
            .startTransmit(*aud_med);
        aud_med->startTransmit(
            Endpoint::instance().audDevManager().getCaptureDevMedia());
      } catch (Error &err) {
        qWarning() << "Error connecting audio:"
                   << QString::fromStdString(err.info());
      }
    } else if (ci.media[i].type == PJMEDIA_TYPE_VIDEO &&
               ci.media[i].status == PJSUA_CALL_MEDIA_ACTIVE) {
      hasVideo = true;
    }
  }

  CallService::instance().callMediaStateChanged(getId(), hasVideo);
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
  connect(this, &CallService::callStateChanged, [](int id, QString state) {
    CallModel::instance().updateCallState(id, state);
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
  try {
    call->makeCall(destination.toStdString(), prm);
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
  CallModel::instance().removeCall(callId);
}

void CallService::toggleMute(int callId) {
  // Mute implementation
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
      m_activeCalls[callId]->reinvite(prm); // Reinvite is often used to resume
      // Or setHold(prm) with false if supported in this version's prm
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
