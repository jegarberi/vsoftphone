#ifndef CALLSERVICE_H
#define CALLSERVICE_H

#include <QMap>
#include <QObject>
#include <QRect>
#include <QSet>
#include <QUrl>
#include <QVariantMap>
#include <map>
#include <memory>
#include <pjsua2.hpp>

class MyCall : public pj::Call {
public:
  MyCall(pj::Account &acc, int call_id = PJSUA_INVALID_ID);
  ~MyCall();

  virtual void onCallState(pj::OnCallStateParam &prm) override;
  virtual void onCallMediaState(pj::OnCallMediaStateParam &prm) override;

  bool silent = false; // not written to the call history
};

class CallService : public QObject {
  Q_OBJECT
  // False when PJSIP was built without video, as in the portable build
  Q_PROPERTY(bool videoAvailable READ videoAvailable CONSTANT)

public:
  bool videoAvailable() const { return PJSUA_HAS_VIDEO; }

  static CallService &instance();

  Q_INVOKABLE void makeCall(const QString &accountId,
                            const QString &destination, bool video = false);
  Q_INVOKABLE void answerCall(int callId);
  Q_INVOKABLE void hangupCall(int callId);
  Q_INVOKABLE void toggleMute(int callId);
  // Starts or stops recording the call to a WAV file; returns the new state
  Q_INVOKABLE bool toggleRecording(int callId);
  Q_INVOKABLE QString recordingsDir() const;
  Q_INVOKABLE QUrl recordingsUrl() const;
  // True when a call other than `exceptCallId` is already connected
  Q_INVOKABLE bool hasConnectedCall(int exceptCallId) const;
  Q_INVOKABLE void holdCall(int callId);
  Q_INVOKABLE void resumeCall(int callId);
  Q_INVOKABLE void transferCall(int callId, const QString &destination);
  Q_INVOKABLE void attendedTransferCall(int callId, int destinationCallId);
  Q_INVOKABLE void sendDtmf(int callId, const QString &digits);

  Q_INVOKABLE void addToConference(int callId);
  Q_INVOKABLE void removeFromConference(int callId);
  bool isConferenceActive() const;

  void addCall(std::shared_ptr<MyCall> call);
  // Takes a ringing call, lists it, starts the ring tone and announces it
  void announceIncoming(std::shared_ptr<MyCall> call, const QString &remoteUri);
  void removeCall(int callId);
  void handleMediaState(int callId);

  int lastCallId() const { return m_lastCallId; }
  QString lastRemoteUri() const { return m_lastRemoteUri; }

  // Android: where on the screen (in pixels) the call window wants the
  // other side's picture; an empty rectangle takes it away
  Q_INVOKABLE void setVideoArea(int x, int y, int width, int height);
  // Android: the surface to draw that picture on (an ANativeWindow), or null
  void setVideoSurface(void *window);

signals:
  void incomingCall(int callId, QString remoteUri);
  void outgoingCall(int callId, QString remoteUri);
  void callStateChanged(int callId, QString state, int status = 0,
                        QString reason = "");
  void callMediaStateChanged(int callId, bool hasVideo);
  // The call could not even be started (no account, malformed address)
  void callFailed(QString destination, QString reason);

private:
  explicit CallService(QObject *parent = nullptr);
  ~CallService();

  QMap<int, std::shared_ptr<MyCall>> m_activeCalls;
  void showVideo();
  void placeVideo();
  int videoWindowId() const;
  // Where the call window wants the picture, and where the surface was put
  QRect m_videoArea, m_videoPlaced;
  void *m_videoSurface = nullptr;
  // The video window that already draws on m_videoSurface, or -1
  int m_videoShownIn = -1;
  QSet<int> m_conferenceParticipants;
  QSet<int> m_mutedCalls;
  QSet<int> m_ringingCalls;
  QSet<int> m_heldCalls;
  QSet<int> m_outgoingCalls;
  std::map<int, std::unique_ptr<pj::AudioMediaRecorder>> m_recorders;
  std::unique_ptr<pj::ToneGenerator> m_ringTone;

  void setRinging(int callId, bool ringing);
  void holdOthers(int exceptCallId);
  int m_lastCallId = -1;
  QString m_lastRemoteUri;
};

#endif // CALLSERVICE_H
