#include "HistoryModel.h"
#include "../pjsip_wrapper/StorageService.h"

HistoryModel::HistoryModel(QObject *parent) : QAbstractListModel(parent) {
  QList<CallRecord> records = StorageService::instance().getCallHistory();
  for (const auto &rec : records) {
    m_history.append({QString::number(rec.id), rec.remoteParty, rec.direction,
                      rec.timestamp, rec.duration});
  }
}

int HistoryModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_history.count();
}

QVariant HistoryModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_history.count())
    return QVariant();

  const CallHistoryEntry &entry = m_history[index.row()];

  switch (role) {
  case IdRole:
    return entry.id;
  case RemotePartyRole:
    return entry.remoteParty;
  case DirectionRole:
    return entry.direction;
  case TimestampRole:
    return entry.timestamp.toString(Qt::TextDate);
  case DurationRole:
    return entry.duration;
  default:
    return QVariant();
  }
}

QHash<int, QByteArray> HistoryModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[IdRole] = "id";
  roles[RemotePartyRole] = "remoteParty";
  roles[DirectionRole] = "direction";
  roles[TimestampRole] = "timestamp";
  roles[DurationRole] = "duration";
  return roles;
}

void HistoryModel::addEntry(const QString &remoteParty,
                            const QString &direction, int duration) {
  StorageService::instance().addCall(remoteParty, direction, duration);

  beginInsertRows(QModelIndex(), 0, 0); // Add to top
  m_history.prepend({QString::number(m_history.count() + 1), remoteParty,
                     direction, QDateTime::currentDateTime(), duration});
  endInsertRows();
}

void HistoryModel::clearHistory() {
  StorageService::instance().clearHistory();
  beginResetModel();
  m_history.clear();
  endResetModel();
}
