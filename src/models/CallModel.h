#ifndef CALLMODEL_H
#define CALLMODEL_H

#include <QAbstractListModel>
#include <QVector>

struct CallData {
  int id;
  QString remoteUri;
  QString state;
  bool hasVideo;
  bool isHeld;
  bool isInConference;
  int lastStatus;
  QString lastReason;
};

class CallModel : public QAbstractListModel {
  Q_OBJECT
public:
  enum CallRoles {
    IdRole = Qt::UserRole + 1,
    RemoteUriRole,
    StateRole,
    HasVideoRole,
    IsHeldRole,
    IsInConferenceRole,
    StatusRole,
    ReasonRole
  };

  static CallModel &instance();

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  void addCall(int id, const QString &remoteUri);
  void updateCallState(int id, const QString &state, int status = 0,
                       const QString &reason = "");
  void updateCallMedia(int id, bool hasVideo);
  void updateCallHold(int id, bool isHeld);
  void updateCallConference(int id, bool isInConference);
  void removeCall(int id);

private:
  explicit CallModel(QObject *parent = nullptr);
  QVector<CallData> m_calls;
};

#endif // CALLMODEL_H
