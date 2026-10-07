#include "Keyring.h"

#include <QDebug>
#include <QStandardPaths>

#ifdef HAVE_LIBSECRET
// GLib has struct members called "signals", which Qt defines as a keyword
#undef signals
#include <libsecret/secret.h>

static const SecretSchema *schema() {
  static const SecretSchema s = {
      "dev.antigravity.MicrosipClone",
      SECRET_SCHEMA_NONE,
      {{"key", SECRET_SCHEMA_ATTRIBUTE_STRING},
       {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING}},
      0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
  return &s;
}

static bool failed(GError *error, const char *what) {
  if (!error)
    return false;
  qWarning() << "Keyring" << what << "failed:" << error->message;
  g_error_free(error);
  return true;
}

bool Keyring::available() {
  // Tests must never touch, or wait on, the user's real keyring
  return !QStandardPaths::isTestModeEnabled() &&
         !qEnvironmentVariableIsSet("MICROSIP_NO_KEYRING");
}

bool Keyring::store(const QString &key, const QString &secret) {
  if (!available())
    return false;
  GError *error = nullptr;
  const gboolean ok = secret_password_store_sync(
      schema(), SECRET_COLLECTION_DEFAULT, "MicrosipClone SIP password",
      secret.toUtf8().constData(), nullptr, &error, "key",
      key.toUtf8().constData(), nullptr);
  return !failed(error, "store") && ok;
}

QString Keyring::lookup(const QString &key, bool *found) {
  *found = false;
  if (!available())
    return QString();
  GError *error = nullptr;
  gchar *secret = secret_password_lookup_sync(
      schema(), nullptr, &error, "key", key.toUtf8().constData(), nullptr);
  if (failed(error, "lookup") || !secret)
    return QString();
  *found = true;
  const QString result = QString::fromUtf8(secret);
  secret_password_free(secret);
  return result;
}

void Keyring::clear(const QString &key) {
  if (!available())
    return;
  GError *error = nullptr;
  secret_password_clear_sync(schema(), nullptr, &error, "key",
                             key.toUtf8().constData(), nullptr);
  failed(error, "clear");
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
