#ifndef PRESENCESERVICE_H
#define PRESENCESERVICE_H

#include <QObject>
#include <QString>

class PresenceService : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString status READ status WRITE setStatus NOTIFY statusChanged)

public:
  enum Status { Online, Away, Busy, Offline };
  Q_ENUM(Status)

  static PresenceService &instance();

  QString status() const { return m_status; }
  void setStatus(const QString &newStatus);
  // Publishes the current status on every account, e.g. after one was added
  void apply();

signals:
  void statusChanged(const QString &status);

private:
  explicit PresenceService(QObject *parent = nullptr);
  QString m_status;
};

#endif // PRESENCESERVICE_H
