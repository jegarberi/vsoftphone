#ifndef BLFSERVICE_H
#define BLFSERVICE_H

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QObject>

struct pjsip_evsub;

struct BlfLamp {
  QString uri;   // monitored user, bare "user@domain"
  QString name;  // display name from the resource list, may be empty
  QString state; // Idle, Ringing or Busy
};

// BroadWorks Busy Lamp Field: one SUBSCRIBE (Event: dialog, Supported:
// eventlist) to the account's BLF list URI. The server answers with NOTIFYs
// carrying a resource list (RFC 4662) of dialog-info documents (RFC 4235),
// one per monitored user. A plain single-user dialog subscription works too.
class BlfService : public QObject {
  Q_OBJECT
public:
  static BlfService &instance();

  // Replaces the account's subscription; an empty URI just removes it
  void subscribe(const QString &accountId, const QString &listUri);
  void unsubscribe(const QString &accountId);

  // `boundary` is the multipart boundary, empty for a single dialog-info body
  static QList<BlfLamp> parseNotify(const QByteArray &boundary,
                                    const QByteArray &body);

  // Which account a subscription belongs to; callable from the PJSIP thread
  QString accountOf(pjsip_evsub *sub);

signals:
  void lampChanged(QString accountId, QString uri, QString name,
                   QString state);
  // The account's subscription is gone: its lamps are no longer known
  void lampsCleared(QString accountId);

private:
  explicit BlfService(QObject *parent = nullptr);
  void subscriptionEnded(pjsip_evsub *sub);

  QMutex m_mutex; // m_subs is also touched from the PJSIP thread
  QMap<QString, pjsip_evsub *> m_subs;
  QMap<QString, QString> m_wanted; // accountId -> list URI
};

#endif // BLFSERVICE_H
