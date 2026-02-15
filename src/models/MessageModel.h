#ifndef MESSAGEMODEL_H
#define MESSAGEMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>

struct MessageItem {
  int id;
  QString remoteParty;
  QString direction;
  QString content;
  QDateTime timestamp;
};

class MessageModel : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(QString currentRemoteParty READ currentRemoteParty WRITE
                 setCurrentRemoteParty NOTIFY currentRemotePartyChanged)

public:
  enum MessageRoles {
    IdRole = Qt::UserRole + 1,
    RemotePartyRole,
    DirectionRole,
    ContentRole,
    TimestampRole
  };

  explicit MessageModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  QString currentRemoteParty() const { return m_currentRemoteParty; }
  void setCurrentRemoteParty(const QString &remoteParty);

  Q_INVOKABLE void sendMessage(const QString &content);

signals:
  void currentRemotePartyChanged();

private:
  void loadMessages();

  QString m_currentRemoteParty;
  QList<MessageItem> m_messages;
};

#endif // MESSAGEMODEL_H
