#include "CallModel.h"

CallModel &CallModel::instance() {
  static CallModel _instance;
  return _instance;
}

CallModel::CallModel(QObject *parent) : QAbstractListModel(parent) {}

int CallModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_calls.count();
}

QVariant CallModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_calls.count())
    return QVariant();

  const CallData &call = m_calls[index.row()];
  switch (role) {
  case IdRole:
    return call.id;
  case RemoteUriRole:
    return call.remoteUri;
  case StateRole:
    return call.state;
  case HasVideoRole:
    return call.hasVideo;
  case IsHeldRole:
    return call.isHeld;
  case IsInConferenceRole:
    return call.isInConference;
  case StatusRole:
    return call.lastStatus;
  case ReasonRole:
    return call.lastReason;
  }
  return QVariant();
}

QHash<int, QByteArray> CallModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[IdRole] = "callId";
  roles[RemoteUriRole] = "remoteUri";
  roles[StateRole] = "state";
  roles[HasVideoRole] = "hasVideo";
  roles[IsHeldRole] = "isHeld";
  roles[IsInConferenceRole] = "isInConference";
  roles[StatusRole] = "lastStatus";
  roles[ReasonRole] = "lastReason";
  return roles;
}

void CallModel::addCall(int id, const QString &remoteUri) {
  beginInsertRows(QModelIndex(), m_calls.count(), m_calls.count());
  m_calls.append({id, remoteUri, "INCOMING", false, false, false, 0, ""});
  endInsertRows();
  emit callsChanged();
}

void CallModel::updateCallState(int id, const QString &state, int status,
                                const QString &reason) {
  for (int i = 0; i < m_calls.count(); ++i) {
    if (m_calls[i].id == id) {
      m_calls[i].state = state;
      m_calls[i].lastStatus = status;
      m_calls[i].lastReason = reason;
      emit dataChanged(index(i), index(i), {StateRole, StatusRole, ReasonRole});
      emit callsChanged();
      break;
    }
  }
}

void CallModel::updateCallMedia(int id, bool hasVideo) {
  for (int i = 0; i < m_calls.count(); ++i) {
    if (m_calls[i].id == id) {
      m_calls[i].hasVideo = hasVideo;
      emit dataChanged(index(i), index(i), {HasVideoRole});
      emit callsChanged();
      break;
    }
  }
}

void CallModel::updateCallHold(int id, bool isHeld) {
  for (int i = 0; i < m_calls.count(); ++i) {
    if (m_calls[i].id == id) {
      m_calls[i].isHeld = isHeld;
      emit dataChanged(index(i), index(i), {IsHeldRole});
      emit callsChanged();
      break;
    }
  }
}

void CallModel::updateCallConference(int id, bool isInConference) {
  for (int i = 0; i < m_calls.count(); ++i) {
    if (m_calls[i].id == id) {
      m_calls[i].isInConference = isInConference;
      emit dataChanged(index(i), index(i), {IsInConferenceRole});
      emit callsChanged();
      break;
    }
  }
}

void CallModel::removeCall(int id) {
  for (int i = 0; i < m_calls.count(); ++i) {
    if (m_calls[i].id == id) {
      beginRemoveRows(QModelIndex(), i, i);
      m_calls.removeAt(i);
      endRemoveRows();
      emit callsChanged();
      break;
    }
  }
}

int CallModel::activeCallId() const {
  for (const CallData &call : m_calls) {
    if (!call.isHeld && call.state != "DISCONNECTED" && call.state != "INCOMING")
      return call.id;
  }
  return -1;
}
