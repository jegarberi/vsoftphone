#ifndef HISTORYMODEL_H
#define HISTORYMODEL_H

#include <QAbstractListModel>
#include <QDateTime>

struct CallHistoryEntry {
  QString id;
  QString remoteParty;
  QString direction; // Incoming, Outgoing, Missed
  QDateTime timestamp;
  int duration; // in seconds
};

class HistoryModel : public QAbstractListModel {
  Q_OBJECT
public:
  enum HistoryRoles {
    IdRole = Qt::UserRole + 1,
    RemotePartyRole,
    DirectionRole,
    TimestampRole,
    DurationRole
  };

  explicit HistoryModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  Q_INVOKABLE void addEntry(const QString &remoteParty,
                            const QString &direction, int duration);
  Q_INVOKABLE void clearHistory();

private:
  void reload();

  QList<CallHistoryEntry> m_history;
};

#endif // HISTORYMODEL_H
