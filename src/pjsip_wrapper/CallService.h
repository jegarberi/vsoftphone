#ifndef CALLSERVICE_H
#define CALLSERVICE_H

#include <QObject>
#include <QSet>
#include <QVariantMap>
#include <memory>
#include <pjsua2.hpp>

class MyCall : public pj::Call {
public:
  MyCall(pj::Account &acc, int call_id = PJSUA_INVALID_ID);
  ~MyCall();

  virtual void onCallState(pj::OnCallStateParam &prm) override;
  virtual void onCallMediaState(pj::OnCallMediaStateParam &prm) override;
};

class CallService : public QObject {
  Q_OBJECT
public:
  static CallService &instance();

  void makeCall(const QString &accountId, const QString &destination);
  void answerCall(int callId);
  void hangupCall(int callId);
  void toggleMute(int callId);
  void holdCall(int callId);
  void resumeCall(int callId);
  void transferCall(int callId, const QString &destination);
  void attendedTransferCall(int callId, int destinationCallId);
  void sendDtmf(int callId, const QString &digits);

  void addToConference(int callId);
  void removeFromConference(int callId);
  bool isConferenceActive() const;

  void addCall(std::shared_ptr<MyCall> call);
  void removeCall(int callId);

  int lastCallId() const { return m_lastCallId; }
  QString lastRemoteUri() const { return m_lastRemoteUri; }

signals:
  void incomingCall(int callId, QString remoteUri);
  void outgoingCall(int callId, QString remoteUri);
  void callStateChanged(int callId, QString state);
  void callMediaStateChanged(int callId, bool hasVideo);

private:
  explicit CallService(QObject *parent = nullptr);
  ~CallService();

  QMap<int, std::shared_ptr<MyCall>> m_activeCalls;
  QSet<int> m_conferenceParticipants;
  int m_lastCallId = -1;
  QString m_lastRemoteUri;
};

#endif // CALLSERVICE_H
