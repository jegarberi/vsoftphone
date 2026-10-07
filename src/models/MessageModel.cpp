#include "MessageModel.h"
#include "../pjsip_wrapper/SipService.h"
#include "../pjsip_wrapper/StorageService.h"
#include "AccountModel.h"
#include <QDebug>

MessageModel::MessageModel(QObject *parent) : QAbstractListModel(parent) {}

int MessageModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_messages.count();
}

QVariant MessageModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_messages.count())
    return QVariant();

  const MessageItem &msg = m_messages[index.row()];

  switch (role) {
  case IdRole:
    return msg.id;
  case RemotePartyRole:
    return msg.remoteParty;
  case DirectionRole:
    return msg.direction;
  case ContentRole:
    return msg.content;
  case TimestampRole:
    return msg.timestamp.toString(Qt::TextDate);
  default:
    return QVariant();
  }
}

QHash<int, QByteArray> MessageModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[IdRole] = "id";
  roles[RemotePartyRole] = "remoteParty";
  roles[DirectionRole] = "direction";
  roles[ContentRole] = "content";
  roles[TimestampRole] = "timestamp";
  return roles;
}

void MessageModel::setCurrentRemoteParty(const QString &remoteParty) {
  if (m_currentRemoteParty == remoteParty)
    return;

  m_currentRemoteParty = remoteParty;
  loadMessages();
  emit currentRemotePartyChanged();
}

void MessageModel::loadMessages() {
  beginResetModel();
  m_messages.clear();
  if (!m_currentRemoteParty.isEmpty()) {
    QList<MessageRecord> records =
        StorageService::instance().getMessages(m_currentRemoteParty);
    for (const auto &rec : records) {
      m_messages.append(
          {rec.id, rec.remoteParty, rec.direction, rec.content, rec.timestamp});
    }
  }
  endResetModel();
}

void MessageModel::sendMessage(const QString &content) {
  if (m_currentRemoteParty.isEmpty() || content.isEmpty())
    return;

  // SipService stores the message once it has been handed to the stack
  SipService::instance().sendMessage(AccountModel::instance().defaultAccountId(),
                                     m_currentRemoteParty, content);
  loadMessages();
}
