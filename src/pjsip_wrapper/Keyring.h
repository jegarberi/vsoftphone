#ifndef KEYRING_H
#define KEYRING_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThread>
#include <functional>

// The operating system's password store: the Secret Service keyring on Linux
// (through libsecret, loaded at run time) and the Credential Manager on
// Windows. Everything fails soft: where there is no store, or in tests,
// callers keep the secret where it was.
namespace Keyring {
bool available();
bool store(const QString &key, const QString &secret);
// `found` tells a stored empty secret from a missing one
QString lookup(const QString &key, bool *found);
void clear(const QString &key);
} // namespace Keyring

// What the application uses: the same store behind a cache, with every
// Keyring call made on a worker thread. A locked keyring can sit on an unlock
// prompt for as long as the user likes; that must not freeze the window.
//
// Reads come from the cache, which is filled once by preload(). Writes update
// the cache at once and reach the store in the background; stored() and
// storeFailed() report how that went, so the caller can keep its own fallback
// copy (a database column, a settings key) only when it is needed.
class SecretStore : public QObject {
  Q_OBJECT
public:
  static SecretStore &instance();

  // Looks the keys up in the background, then emits ready(). With no store
  // there is nothing to wait for and it is ready immediately.
  void preload(const QStringList &keys);
  bool isReady() const { return m_ready; }

  QString get(const QString &key, bool *found) const;
  void set(const QString &key, const QString &secret); // empty removes it
  void remove(const QString &key);

  // Key of the global TURN password (the account ones are StorageService's)
  static const QString TurnPasswordKey;

signals:
  void ready();
  void stored(QString key);
  void storeFailed(QString key);

private:
  SecretStore();
  ~SecretStore();
  void onWorker(std::function<void()> job);
  void stop();

  QHash<QString, QString> m_cache;
  bool m_ready = false;
  QThread m_thread;
  QObject *m_worker;
};

#endif // KEYRING_H
