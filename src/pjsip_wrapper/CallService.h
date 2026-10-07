#ifndef CALLSERVICE_H
#define CALLSERVICE_H

#include <QMap>
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

  Q_INVOKABLE void makeCall(const QString &accountId,
                            const QString &destination);
  Q_INVOKABLE void answerCall(int callId);
  Q_INVOKABLE void hangupCall(int callId);
  Q_INVOKABLE void toggleMute(int callId);
  Q_INVOKABLE void holdCall(int callId);
  Q_INVOKABLE void resumeCall(int callId);
  Q_INVOKABLE void transferCall(int callId, const QString &destination);
  Q_INVOKABLE void attendedTransferCall(int callId, int destinationCallId);
  Q_INVOKABLE void sendDtmf(int callId, const QString &digits);

  Q_INVOKABLE void addToConference(int callId);
  Q_INVOKABLE void removeFromConference(int callId);
  bool isConferenceActive() const;

  void addCall(std::shared_ptr<MyCall> call);
  void removeCall(int callId);
  void handleMediaState(int callId);

  int lastCallId() const { return m_lastCallId; }
  QString lastRemoteUri() const { return m_lastRemoteUri; }

signals:
  void incomingCall(int callId, QString remoteUri);
  void outgoingCall(int callId, QString remoteUri);
  void callStateChanged(int callId, QString state, int status = 0,
                        QString reason = "");
  void callMediaStateChanged(int callId, bool hasVideo);

private:
  explicit CallService(QObject *parent = nullptr);
  ~CallService();

  QMap<int, std::shared_ptr<MyCall>> m_activeCalls;
  QSet<int> m_conferenceParticipants;
  QSet<int> m_mutedCalls;
  int m_lastCallId = -1;
  QString m_lastRemoteUri;
};

#endif // CALLSERVICE_H
