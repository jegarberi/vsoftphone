#include "Keyring.h"

#include <QDebug>
#include <QStandardPaths>

// Tests must never touch, or wait on, the user's real password store, unless
// they ask to with MICROSIP_TEST_KEYRING (the store's own round-trip check).
static bool allowed() {
  if (qEnvironmentVariableIsSet("MICROSIP_NO_KEYRING"))
    return false;
  return !QStandardPaths::isTestModeEnabled() ||
         qEnvironmentVariableIsSet("MICROSIP_TEST_KEYRING");
}

#if defined(Q_OS_LINUX)

#include <dlfcn.h>

// Just enough of libsecret's public ABI to store, look up and clear one
// password. It is loaded at run time instead of linked, so the portable
// build, which cannot link GLib, uses the keyring as well.
namespace {
struct SchemaAttribute {
  const char *name;
  int type; // 0 = string
};
struct Schema {
  const char *name;
  int flags;
  SchemaAttribute attributes[32];
  int reserved;
  void *reservedPointers[7];
};
struct GError {
  quint32 domain;
  int code;
  char *message;
};

struct LibSecret {
  int (*store)(const Schema *, const char *collection, const char *label,
               const char *password, void *cancellable, GError **error,
               ...) = nullptr;
  char *(*lookup)(const Schema *, void *cancellable, GError **error,
                  ...) = nullptr;
  int (*clear)(const Schema *, void *cancellable, GError **error,
               ...) = nullptr;
  void (*freePassword)(char *) = nullptr;
  void (*freeError)(GError *) = nullptr;
  bool loaded = false;
};

const LibSecret &libsecret() {
  static const LibSecret lib = [] {
    LibSecret l;
    void *handle = dlopen("libsecret-1.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (!handle)
      return l;
    l.store = reinterpret_cast<decltype(l.store)>(
        dlsym(handle, "secret_password_store_sync"));
    l.lookup = reinterpret_cast<decltype(l.lookup)>(
        dlsym(handle, "secret_password_lookup_sync"));
    l.clear = reinterpret_cast<decltype(l.clear)>(
        dlsym(handle, "secret_password_clear_sync"));
    l.freePassword = reinterpret_cast<decltype(l.freePassword)>(
        dlsym(handle, "secret_password_free"));
    l.freeError = reinterpret_cast<decltype(l.freeError)>(
        dlsym(handle, "g_error_free"));
    l.loaded =
        l.store && l.lookup && l.clear && l.freePassword && l.freeError;
    return l;
  }();
  return lib;
}

const Schema *schema() {
  static const Schema s = {"dev.antigravity.MicrosipClone", 0, {{"key", 0}},
                           0, {}};
  return &s;
}

bool failed(GError *error, const char *what) {
  if (!error)
    return false;
  qWarning() << "Keyring" << what << "failed:" << error->message;
  libsecret().freeError(error);
  return true;
}
} // namespace

bool Keyring::available() { return allowed() && libsecret().loaded; }

bool Keyring::store(const QString &key, const QString &secret) {
  if (!available())
    return false;
  GError *error = nullptr;
  const int ok = libsecret().store(
      schema(), "default", "MicrosipClone SIP password",
      secret.toUtf8().constData(), nullptr, &error, "key",
      key.toUtf8().constData(), nullptr);
  return !failed(error, "store") && ok;
}

QString Keyring::lookup(const QString &key, bool *found) {
  *found = false;
  if (!available())
    return QString();
  GError *error = nullptr;
  char *secret = libsecret().lookup(schema(), nullptr, &error, "key",
                                    key.toUtf8().constData(), nullptr);
  if (failed(error, "lookup") || !secret)
    return QString();
  *found = true;
  const QString result = QString::fromUtf8(secret);
  libsecret().freePassword(secret);
  return result;
}

void Keyring::clear(const QString &key) {
  if (!available())
    return;
  GError *error = nullptr;
  libsecret().clear(schema(), nullptr, &error, "key",
                    key.toUtf8().constData(), nullptr);
  failed(error, "clear");
}

#elif defined(Q_OS_WIN)

#include <windows.h>
#include <wincred.h>

static std::wstring target(const QString &key) {
  return ("MicrosipClone:" + key).toStdWString();
}

bool Keyring::available() { return allowed(); }

bool Keyring::store(const QString &key, const QString &secret) {
  if (!available())
    return false;
  std::wstring name = target(key);
  QByteArray blob = secret.toUtf8();
  wchar_t user[] = L"MicrosipClone";
  CREDENTIALW credential = {};
  credential.Type = CRED_TYPE_GENERIC;
  credential.TargetName = name.data();
  credential.UserName = user;
  credential.CredentialBlobSize = DWORD(blob.size());
  credential.CredentialBlob = reinterpret_cast<LPBYTE>(blob.data());
  credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
  return CredWriteW(&credential, 0);
}

QString Keyring::lookup(const QString &key, bool *found) {
  *found = false;
  if (!available())
    return QString();
  PCREDENTIALW credential = nullptr;
  if (!CredReadW(target(key).c_str(), CRED_TYPE_GENERIC, 0, &credential))
    return QString();
  *found = true;
  const QString result = QString::fromUtf8(
      reinterpret_cast<const char *>(credential->CredentialBlob),
      int(credential->CredentialBlobSize));
  CredFree(credential);
  return result;
}

void Keyring::clear(const QString &key) {
  if (available())
    CredDeleteW(target(key).c_str(), CRED_TYPE_GENERIC, 0);
}

#else

bool Keyring::available() { return false; }
bool Keyring::store(const QString &, const QString &) { return false; }
QString Keyring::lookup(const QString &, bool *found) {
  *found = false;
  return QString();
}
void Keyring::clear(const QString &) {}

#endif
