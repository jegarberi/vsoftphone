#include "StorageService.h"
#include "Keyring.h"
#include "SipUri.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QThread>

StorageService &StorageService::instance() {
  static StorageService _instance;
  return _instance;
}

StorageService::StorageService(QObject *parent) : QObject(parent) {}

StorageService::~StorageService() {
  // Connections are cleaned up by Qt when the thread ends or connection is
  // removed
}

QSqlDatabase StorageService::database() const {
  QString connectionName =
      QString("microsip_connection_%1")
          .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
  if (QSqlDatabase::contains(connectionName)) {
    return QSqlDatabase::database(connectionName);
  }

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
  db.setDatabaseName(m_dbPath);
  if (!db.open()) {
    qCritical() << "Error opening database in thread"
                << QThread::currentThreadId() << ":" << db.lastError().text();
  }
  return db;
}

QString StorageService::secretKey(int accountId, const char *field) const {
  return QString("%1#%2/%3").arg(m_dbPath).arg(accountId).arg(field);
}

// Returns what to write in the database column: nothing once the keyring has
// taken the secret, the secret itself when there is no keyring.
QString StorageService::protectSecret(int accountId, const char *field,
                                      const QString &value) {
  const QString key = secretKey(accountId, field);
  m_secrets[key] = value;
  if (value.isEmpty()) {
    Keyring::clear(key);
    return value;
  }
  return Keyring::store(key, value) ? QString() : value;
}

QString StorageService::loadSecret(int accountId, const char *field,
                                   const QString &dbValue) {
  const QString key = secretKey(accountId, field);
  auto cached = m_secrets.constFind(key);
  if (cached != m_secrets.constEnd())
    return *cached;

  QString value = dbValue;
  if (!dbValue.isEmpty()) {
    // Plaintext from an older version, or from a time without a keyring:
    // move it over. `field` is one of our own column names.
    if (protectSecret(accountId, field, dbValue).isEmpty()) {
      QSqlQuery query(database());
      query.prepare(QString("UPDATE accounts SET %1 = '' WHERE id = :id")
                        .arg(QLatin1String(field)));
      query.bindValue(":id", accountId);
      query.exec();
    }
  } else {
    bool found = false;
    value = Keyring::lookup(key, &found);
  }
  m_secrets[key] = value;
  return value;
}

bool StorageService::init() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dir(path);
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  m_dbPath = dir.filePath("microsip.db");

  const bool ok = createTables();
  // The database holds SIP passwords: keep it readable by the owner only
  QFile::setPermissions(m_dbPath, QFile::ReadOwner | QFile::WriteOwner);
  return ok;
}

bool StorageService::createTables() {
  QSqlQuery query(database());
  bool success = true;

  success &= query.exec("CREATE TABLE IF NOT EXISTS calls ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                        "remoteParty TEXT,"
                        "direction TEXT,"
                        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
                        "duration INTEGER)");

  success &= query.exec("CREATE TABLE IF NOT EXISTS contacts ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                        "name TEXT,"
                        "number TEXT,"
                        "grp TEXT DEFAULT '')");

  success &= query.exec("CREATE TABLE IF NOT EXISTS messages ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                        "remoteParty TEXT,"
                        "direction TEXT,"
                        "content TEXT,"
                        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)");

  success &= query.exec("CREATE TABLE IF NOT EXISTS accounts ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                        "accountName TEXT,"
                        "server TEXT,"
                        "proxy TEXT,"
                        "username TEXT,"
                        "domain TEXT,"
                        "login TEXT,"
                        "password TEXT,"
                        "displayName TEXT,"
                        "transport TEXT,"
                        "registerRefresh INTEGER,"
                        "publishPresence BOOLEAN,"
                        "iceEnabled BOOLEAN,"
                        "stunServer TEXT,"
                        "turnServer TEXT,"
                        "turnUser TEXT,"
                        "turnPass TEXT,"
                        "srtpMode INTEGER,"
                        "allowIpRewrite BOOLEAN DEFAULT 0,"
                        "autoNat BOOLEAN DEFAULT 0,"
                        "enableStun BOOLEAN DEFAULT 1,"
                        "ipVersion INTEGER DEFAULT 2,"
                        "rport BOOLEAN DEFAULT 0,"
                        "active BOOLEAN,"
                        "blfUri TEXT DEFAULT '')");

  // Migration for existing databases
  query.exec(
      "ALTER TABLE accounts ADD COLUMN allowIpRewrite BOOLEAN DEFAULT 0");
  query.exec("ALTER TABLE accounts ADD COLUMN login TEXT");
  query.exec("ALTER TABLE accounts ADD COLUMN autoNat BOOLEAN DEFAULT 0");
  query.exec("ALTER TABLE accounts ADD COLUMN enableStun BOOLEAN DEFAULT 1");
  query.exec("ALTER TABLE accounts ADD COLUMN ipVersion INTEGER DEFAULT 2");
  query.exec("ALTER TABLE accounts ADD COLUMN rport BOOLEAN DEFAULT 0");
  query.exec("ALTER TABLE contacts ADD COLUMN grp TEXT DEFAULT ''");
  query.exec("ALTER TABLE accounts ADD COLUMN blfUri TEXT DEFAULT ''");

  if (!success) {
    qCritical() << "Error creating tables:" << query.lastError().text();
  }
  return success;
}

void StorageService::addCall(const QString &remoteParty,
                             const QString &direction, int duration) {
  QSqlQuery query(database());
  query.prepare("INSERT INTO calls (remoteParty, direction, duration, "
                "timestamp) VALUES (:nr, :dir, :dur, :ts)");
  query.bindValue(":nr", remoteParty);
  query.bindValue(":dir", direction);
  query.bindValue(":dur", duration);
  query.bindValue(":ts", QDateTime::currentDateTime());

  if (!query.exec()) {
    qWarning() << "Error adding call history:" << query.lastError().text();
  }
  emit callHistoryChanged();
}

QList<CallRecord> StorageService::getCallHistory() {
  QList<CallRecord> list;
  QSqlQuery query(database());
  query.prepare("SELECT id, remoteParty, direction, timestamp, duration FROM "
                "calls ORDER BY id DESC");
  if (query.exec()) {
    while (query.next()) {
      CallRecord rec;
      rec.id = query.value(0).toInt();
      rec.remoteParty = query.value(1).toString();
      rec.direction = query.value(2).toString();
      rec.timestamp = query.value(3).toDateTime();
      rec.duration = query.value(4).toInt();
      list.append(rec);
    }
  }
  return list;
}

void StorageService::clearHistory() {
  QSqlQuery query(database());
  query.prepare("DELETE FROM calls");
  if (!query.exec()) {
    qWarning() << "Error clearing call history:" << query.lastError().text();
  }
}

void StorageService::addContact(const QString &name, const QString &number,
                                const QString &group) {
  QSqlQuery query(database());
  query.prepare("INSERT INTO contacts (name, number, grp) VALUES (:name, "
                ":number, :grp)");
  query.bindValue(":name", name);
  query.bindValue(":number", number);
  query.bindValue(":grp", group);
  if (!query.exec()) {
    qWarning() << "Error adding contact:" << query.lastError().text();
  }
}

void StorageService::removeContact(int id) {
  QSqlQuery query(database());
  query.prepare("DELETE FROM contacts WHERE id = :id");
  query.bindValue(":id", id);
  if (!query.exec()) {
    qWarning() << "Error removing contact:" << query.lastError().text();
  }
}

QList<ContactRecord> StorageService::getContacts() {
  QList<ContactRecord> list;
  QSqlQuery query(database());
  query.prepare("SELECT id, name, number, grp FROM contacts ORDER BY grp, name");
  if (query.exec()) {
    while (query.next()) {
      ContactRecord rec;
      rec.id = query.value(0).toInt();
      rec.name = query.value(1).toString();
      rec.number = query.value(2).toString();
      rec.group = query.value(3).toString();
      list.append(rec);
    }
  }
  return list;
}

void StorageService::addMessage(const QString &remoteParty,
                                const QString &direction,
                                const QString &content) {
  QSqlQuery query(database());
  query.prepare(
      "INSERT INTO messages (remoteParty, direction, content, timestamp) "
      "VALUES (:rp, :dir, :con, :ts)");
  query.bindValue(":rp", bareSipUri(remoteParty));
  query.bindValue(":dir", direction);
  query.bindValue(":con", content);
  query.bindValue(":ts", QDateTime::currentDateTime());
  if (!query.exec()) {
    qWarning() << "Error adding message:" << query.lastError().text();
  }
  emit messagesChanged();
}

void StorageService::addAccount(const AccountRecord &acc) {
  QSqlQuery query(database());
  query.prepare(
      "INSERT INTO accounts (accountName, server, proxy, username, "
      "domain, login, password, displayName, transport, registerRefresh, "
      "publishPresence, iceEnabled, stunServer, turnServer, "
      "turnUser, turnPass, srtpMode, allowIpRewrite, autoNat, enableStun, "
      "ipVersion, rport, active, blfUri) VALUES "
      "(:an, :sv, :px, :un, :dn, :ln, :pw, :dn_name, :tr, :rr, :pp, :ie, "
      ":ss, :ts, :tu, :tp, :sm, :ar, :anat, :estun, :ipv, :rp, :ac, :blf)");
  query.bindValue(":an", acc.accountName);
  query.bindValue(":sv", acc.server);
  query.bindValue(":px", acc.proxy);
  query.bindValue(":un", acc.username);
  query.bindValue(":dn", acc.domain);
  query.bindValue(":ln", acc.login);
  query.bindValue(":pw", acc.password);
  query.bindValue(":dn_name", acc.displayName);
  query.bindValue(":tr", acc.transport);
  query.bindValue(":rr", acc.registerRefresh);
  query.bindValue(":pp", acc.publishPresence);
  query.bindValue(":ie", acc.iceEnabled);
  query.bindValue(":ss", acc.stunServer);
  query.bindValue(":ts", acc.turnServer);
  query.bindValue(":tu", acc.turnUser);
  query.bindValue(":tp", acc.turnPass);
  query.bindValue(":sm", acc.srtpMode);
  query.bindValue(":ar", acc.allowIpRewrite);
  query.bindValue(":anat", acc.autoNat);
  query.bindValue(":estun", acc.enableStun);
  query.bindValue(":estun", acc.enableStun);
  query.bindValue(":ipv", acc.ipVersion);
  query.bindValue(":rp", acc.rport);
  query.bindValue(":ac", acc.active);
  query.bindValue(":blf", acc.blfUri);

  if (!query.exec()) {
    qWarning() << "Error adding account:" << query.lastError().text();
    return;
  }
  const int id = query.lastInsertId().toInt();
  loadSecret(id, "password", acc.password);
  loadSecret(id, "turnPass", acc.turnPass);
}

void StorageService::updateAccount(const AccountRecord &acc) {
  QSqlQuery query(database());
  query.prepare(
      "UPDATE accounts SET accountName=:an, server=:sv, proxy=:px, "
      "username=:un, domain=:dn, login=:ln, password=:pw, "
      "displayName=:dn_name, "
      "transport=:tr, registerRefresh=:rr, publishPresence=:pp, "
      "iceEnabled=:ie, stunServer=:ss, turnServer=:ts, turnUser=:tu, "
      "turnPass=:tp, srtpMode=:sm, allowIpRewrite=:ar, "
      "autoNat=:anat, enableStun=:estun, ipVersion=:ipv, rport=:rp, active=:ac, "
      "blfUri=:blf "
      "WHERE id=:id");
  query.bindValue(":an", acc.accountName);
  query.bindValue(":sv", acc.server);
  query.bindValue(":px", acc.proxy);
  query.bindValue(":un", acc.username);
  query.bindValue(":dn", acc.domain);
  query.bindValue(":ln", acc.login);
  query.bindValue(":pw", protectSecret(acc.id, "password", acc.password));
  query.bindValue(":dn_name", acc.displayName);
  query.bindValue(":tr", acc.transport);
  query.bindValue(":rr", acc.registerRefresh);
  query.bindValue(":pp", acc.publishPresence);
  query.bindValue(":ie", acc.iceEnabled);
  query.bindValue(":ss", acc.stunServer);
  query.bindValue(":ts", acc.turnServer);
  query.bindValue(":tu", acc.turnUser);
  query.bindValue(":tp", protectSecret(acc.id, "turnPass", acc.turnPass));
  query.bindValue(":sm", acc.srtpMode);
  query.bindValue(":ar", acc.allowIpRewrite);
  query.bindValue(":anat", acc.autoNat);
  query.bindValue(":estun", acc.enableStun);
  query.bindValue(":estun", acc.enableStun);
  query.bindValue(":ipv", acc.ipVersion);
  query.bindValue(":rp", acc.rport);
  query.bindValue(":ac", acc.active);
  query.bindValue(":blf", acc.blfUri);
  query.bindValue(":id", acc.id);

  if (!query.exec()) {
    qWarning() << "Error updating account:" << query.lastError().text();
  }
}

void StorageService::removeAccount(int id) {
  QSqlQuery query(database());
  for (const char *field : {"password", "turnPass"}) {
    Keyring::clear(secretKey(id, field));
    m_secrets.remove(secretKey(id, field));
  }
  query.prepare("DELETE FROM accounts WHERE id = :id");
  query.bindValue(":id", id);
  if (!query.exec()) {
    qWarning() << "Error removing account:" << query.lastError().text();
  }
}

QList<AccountRecord> StorageService::getAccounts() {
  QList<AccountRecord> list;
  QSqlQuery query(database());
  query.prepare(
      "SELECT id, accountName, server, proxy, username, domain, login, "
      "password, displayName, transport, registerRefresh, "
      "publishPresence, iceEnabled, stunServer, turnServer, "
      "turnUser, turnPass, srtpMode, allowIpRewrite, autoNat, enableStun, "
      "ipVersion, rport, active, blfUri "
      "FROM accounts");
  if (query.exec()) {
    while (query.next()) {
      AccountRecord rec;
      rec.id = query.value(0).toInt();
      rec.accountName = query.value(1).toString();
      rec.server = query.value(2).toString();
      rec.proxy = query.value(3).toString();
      rec.username = query.value(4).toString();
      rec.domain = query.value(5).toString();
      rec.login = query.value(6).toString();
      rec.password = query.value(7).toString();
      rec.displayName = query.value(8).toString();
      rec.transport = query.value(9).toString();
      rec.registerRefresh = query.value(10).toInt();
      rec.publishPresence = query.value(11).toBool();
      rec.iceEnabled = query.value(12).toBool();
      rec.stunServer = query.value(13).toString();
      rec.turnServer = query.value(14).toString();
      rec.turnUser = query.value(15).toString();
      rec.turnPass = query.value(16).toString();
      rec.srtpMode = query.value(17).toInt();
      rec.allowIpRewrite = query.value(18).toBool();
      rec.autoNat = query.value(19).toBool();
      rec.enableStun = query.value(20).toBool();
      rec.ipVersion = query.value(21).toInt();
      rec.rport = query.value(22).toBool();
      rec.active = query.value(23).toBool();
      rec.blfUri = query.value(24).toString();
      list.append(rec);
    }
  }
  // After the loop: moving a secret writes to the table being read
  for (AccountRecord &rec : list) {
    rec.password = loadSecret(rec.id, "password", rec.password);
    rec.turnPass = loadSecret(rec.id, "turnPass", rec.turnPass);
  }
  return list;
}

QList<MessageRecord> StorageService::getMessages(const QString &remoteParty) {
  QList<MessageRecord> list;
  QSqlQuery query(database());
  query.prepare("SELECT id, remoteParty, direction, content, timestamp FROM "
                "messages WHERE remoteParty = :rp OR remoteParty LIKE :pat "
                "ORDER BY id ASC");
  // A contact saved as a bare number also owns "number@anyhost"
  const QString key = bareSipUri(remoteParty);
  query.bindValue(":rp", key);
  query.bindValue(":pat", key.contains('@') ? key : key + "@%");
  if (query.exec()) {
    while (query.next()) {
      MessageRecord rec;
      rec.id = query.value(0).toInt();
      rec.remoteParty = query.value(1).toString();
      rec.direction = query.value(2).toString();
      rec.content = query.value(3).toString();
      rec.timestamp = query.value(4).toDateTime();
      list.append(rec);
    }
  }
  return list;
}

void StorageService::markMessageFailed(const QString &toUri,
                                       const QString &content) {
  const QString full = bareSipUri(toUri);
  QSqlQuery query(database());
  // The row may be keyed by the bare number the user typed
  query.prepare("UPDATE messages SET direction = 'Failed' WHERE id = ("
                "SELECT MAX(id) FROM messages WHERE direction = 'Outgoing' "
                "AND content = :content AND (remoteParty = :full "
                "OR :full2 LIKE remoteParty || '@%'))");
  query.bindValue(":content", content);
  query.bindValue(":full", full);
  query.bindValue(":full2", full);
  if (!query.exec())
    qWarning() << "Error marking message failed:" << query.lastError().text();
  emit messagesChanged();
}

QStringList StorageService::conversations() {
  QStringList parties;
  QSqlQuery query(database());
  if (query.exec("SELECT remoteParty FROM messages GROUP BY remoteParty "
                 "ORDER BY MAX(id) DESC")) {
    while (query.next())
      parties.append(query.value(0).toString());
  }
  // "1001@pbx" belongs to the conversation "1001" when that one exists
  QStringList result;
  for (const QString &party : parties) {
    if (!party.contains('@') || !parties.contains(party.section('@', 0, 0)))
      result.append(party);
  }
  return result;
}
