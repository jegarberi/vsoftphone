#include "StorageService.h"
#include <QDebug>
#include <QDir>
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

bool StorageService::init() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dir(path);
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  m_dbPath = dir.filePath("microsip.db");

  // Verify connection by creating tables
  return createTables();
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
                        "number TEXT)");

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
                        "active BOOLEAN)");

  // Migration for existing databases
  query.exec(
      "ALTER TABLE accounts ADD COLUMN allowIpRewrite BOOLEAN DEFAULT 0");
  query.exec("ALTER TABLE accounts ADD COLUMN login TEXT");
  query.exec("ALTER TABLE accounts ADD COLUMN autoNat BOOLEAN DEFAULT 0");
  query.exec("ALTER TABLE accounts ADD COLUMN enableStun BOOLEAN DEFAULT 1");
  query.exec("ALTER TABLE accounts ADD COLUMN ipVersion INTEGER DEFAULT 2");
  query.exec("ALTER TABLE accounts ADD COLUMN rport BOOLEAN DEFAULT 0");

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

void StorageService::addContact(const QString &name, const QString &number) {
  QSqlQuery query(database());
  query.prepare("INSERT INTO contacts (name, number) VALUES (:name, :number)");
  query.bindValue(":name", name);
  query.bindValue(":number", number);
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
  query.prepare("SELECT id, name, number FROM contacts");
  if (query.exec()) {
    while (query.next()) {
      ContactRecord rec;
      rec.id = query.value(0).toInt();
      rec.name = query.value(1).toString();
      rec.number = query.value(2).toString();
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
  query.bindValue(":rp", remoteParty);
  query.bindValue(":dir", direction);
  query.bindValue(":con", content);
  query.bindValue(":ts", QDateTime::currentDateTime());
  if (!query.exec()) {
    qWarning() << "Error adding message:" << query.lastError().text();
  }
}

void StorageService::addAccount(const AccountRecord &acc) {
  QSqlQuery query(database());
  query.prepare(
      "INSERT INTO accounts (accountName, server, proxy, username, "
      "domain, login, password, displayName, transport, registerRefresh, "
      "publishPresence, iceEnabled, stunServer, turnServer, "
      "turnUser, turnPass, srtpMode, allowIpRewrite, autoNat, enableStun, "
      "ipVersion, rport, active) VALUES "
      "(:an, :sv, :px, :un, :dn, :ln, :pw, :dn_name, :tr, :rr, :pp, :ie, "
      ":ss, :ts, :tu, :tp, :sm, :ar, :anat, :estun, :ipv, :rp, :ac)");
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

  if (!query.exec()) {
    qWarning() << "Error adding account:" << query.lastError().text();
  }
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
      "autoNat=:anat, enableStun=:estun, ipVersion=:ipv, rport=:rp, active=:ac "
      "WHERE id=:id");
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
  query.bindValue(":id", acc.id);

  if (!query.exec()) {
    qWarning() << "Error updating account:" << query.lastError().text();
  }
}

void StorageService::removeAccount(int id) {
  QSqlQuery query(database());
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
      "ipVersion, rport, active "
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
      list.append(rec);
    }
  }
  return list;
}

QList<MessageRecord> StorageService::getMessages(const QString &remoteParty) {
  QList<MessageRecord> list;
  QSqlQuery query(database());
  query.prepare("SELECT id, remoteParty, direction, content, timestamp FROM "
                "messages WHERE remoteParty = :rp ORDER BY id ASC");
  query.bindValue(":rp", remoteParty);
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
