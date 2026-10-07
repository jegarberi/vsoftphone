#ifndef KEYRING_H
#define KEYRING_H

#include <QString>

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

#endif // KEYRING_H
