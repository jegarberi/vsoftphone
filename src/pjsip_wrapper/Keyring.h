#ifndef KEYRING_H
#define KEYRING_H

#include <QString>

// The desktop keyring (Secret Service, through libsecret). Everything fails
// soft: without libsecret, without a keyring daemon, or in tests, callers fall
// back to keeping the secret where it was.
namespace Keyring {
bool available();
bool store(const QString &key, const QString &secret);
// `found` tells a stored empty secret from a missing one
QString lookup(const QString &key, bool *found);
void clear(const QString &key);
} // namespace Keyring

#endif // KEYRING_H
