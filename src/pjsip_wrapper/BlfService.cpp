#include "BlfService.h"
#include "SipService.h"
#include "SipUri.h"

#include <QDebug>
#include <QRegularExpression>
#include <QTimer>
#include <QXmlStreamReader>
#include <pjsip.h>
#include <pjsip_simple.h>
#include <pjsip_ua.h>
#include <pjsua-lib/pjsua.h>

// ---------------------------------------------------------------------------
// NOTIFY body parsing
// ---------------------------------------------------------------------------

// One dialog-info document describes one monitored user.
// ponytail: every document is read as the user's complete state. BroadWorks
// sends them that way; honour state="partial" if a server ever does not.
static bool parseDialogInfo(const QByteArray &xml, BlfLamp &lamp) {
  QXmlStreamReader reader(xml);
  bool found = false, ringing = false, busy = false;
  while (!reader.atEnd()) {
    if (reader.readNext() != QXmlStreamReader::StartElement)
      continue;
    if (reader.name() == QLatin1String("dialog-info")) {
      lamp.uri = bareSipUri(reader.attributes().value("entity").toString());
      found = !lamp.uri.isEmpty();
    } else if (reader.name() == QLatin1String("state")) {
      const QString state = reader.readElementText().trimmed().toLower();
      if (state == "confirmed")
        busy = true;
      else if (state == "early" || state == "proceeding" || state == "trying")
        ringing = true;
    }
  }
  lamp.state = busy ? "Busy" : ringing ? "Ringing" : "Idle";
  return found;
}

// The list index: maps each monitored URI to its display name
static QMap<QString, QString> parseRlmiNames(const QByteArray &xml) {
  QMap<QString, QString> names;
  QXmlStreamReader reader(xml);
  QString uri;
  while (!reader.atEnd()) {
    if (reader.readNext() != QXmlStreamReader::StartElement)
      continue;
    if (reader.name() == QLatin1String("resource"))
      uri = bareSipUri(reader.attributes().value("uri").toString());
    else if (reader.name() == QLatin1String("name") && !uri.isEmpty())
      names[uri] = reader.readElementText().trimmed();
  }
  return names;
}

QList<BlfLamp> BlfService::parseNotify(const QByteArray &boundary,
                                       const QByteArray &body) {
  QList<BlfLamp> lamps;
  BlfLamp lamp;
  if (boundary.isEmpty()) {
    if (parseDialogInfo(body, lamp))
      lamps.append(lamp);
    return lamps;
  }

  QMap<QString, QString> names;
  const QByteArray delimiter = "--" + boundary;
  int pos = body.indexOf(delimiter);
  while (pos >= 0) {
    const int start = pos + delimiter.size();
    const int next = body.indexOf(delimiter, start);
    if (next < 0)
      break; // what follows the closing delimiter is not a part
    const QByteArray part = body.mid(start, next - start);
    pos = next;

    int split = part.indexOf("\r\n\r\n");
    int skip = 4;
    if (split < 0) {
      split = part.indexOf("\n\n");
      skip = 2;
    }
    if (split < 0)
      continue;
    const QByteArray headers = part.left(split).toLower();
    const QByteArray content = part.mid(split + skip);
    if (headers.contains("application/rlmi+xml"))
      names = parseRlmiNames(content);
    else if (headers.contains("application/dialog-info+xml") &&
             parseDialogInfo(content, lamp))
      lamps.append(lamp);
  }
  for (BlfLamp &l : lamps)
    l.name = names.value(l.uri);
  return lamps;
}

// ---------------------------------------------------------------------------
// Subscription
// ---------------------------------------------------------------------------

static pj_str_t STR_DIALOG = {(char *)"dialog", 6};

static pjsip_module g_blfModule = {
    nullptr, nullptr,                // list links
    {(char *)"mod-blf", 7},          // name
    -1,                              // id
    PJSIP_MOD_PRIORITY_DIALOG_USAGE, // priority
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr};

// Registers the "dialog" event package so that every SUBSCRIBE, refreshes
// included, accepts resource lists, and advertises "eventlist" support.
static bool ensurePackage() {
  static bool ready = false;
  if (ready)
    return true;
  pjsip_endpoint *endpt = pjsua_get_pjsip_endpt();
  if (pjsip_endpt_register_module(endpt, &g_blfModule) != PJ_SUCCESS)
    return false;
  pj_str_t accept[] = {{(char *)"application/dialog-info+xml", 27},
                       {(char *)"multipart/related", 17},
                       {(char *)"application/rlmi+xml", 20}};
  if (pjsip_evsub_register_pkg(&g_blfModule, &STR_DIALOG, 3600, 3, accept) !=
      PJ_SUCCESS)
    return false;
  pj_str_t eventlist = {(char *)"eventlist", 9};
  pjsip_endpt_add_capability(endpt, &g_blfModule, PJSIP_H_SUPPORTED, nullptr, 1,
                             &eventlist);
  ready = true;
  return true;
}

// PJSIP thread. The body is copied out and parsed on the GUI thread.
static void onRxNotify(pjsip_evsub *, pjsip_rx_data *rdata, int *p_st_code,
                       pj_str_t **, pjsip_hdr *, pjsip_msg_body **) {
  *p_st_code = 200;
  if (pjsua_get_state() != PJSUA_STATE_RUNNING)
    return; // shutting down: the service may already be gone
  const pjsip_msg_body *body = rdata->msg_info.msg->body;
  if (!body || !body->data || !body->len)
    return;

  QByteArray boundary;
  if (pj_stricmp2(&body->content_type.type, "multipart") == 0) {
    pj_str_t name = {(char *)"boundary", 8};
    const pjsip_param *param =
        pjsip_param_find(&body->content_type.param, &name);
    if (param)
      boundary = QByteArray(param->value.ptr, param->value.slen).replace('"', "");
  }
  const QByteArray data(static_cast<const char *>(body->data), body->len);

  QMetaObject::invokeMethod(
      &BlfService::instance(),
      [boundary, data] {
        for (const BlfLamp &lamp : BlfService::parseNotify(boundary, data))
          emit BlfService::instance().lampChanged(lamp.uri, lamp.name,
                                                  lamp.state);
      },
      Qt::QueuedConnection);
}

static void (*g_subscriptionEnded)(pjsip_evsub *) = nullptr;

static void onEvsubState(pjsip_evsub *sub, pjsip_event *) {
  if (pjsip_evsub_get_state(sub) == PJSIP_EVSUB_STATE_TERMINATED &&
      pjsua_get_state() == PJSUA_STATE_RUNNING && g_subscriptionEnded)
    g_subscriptionEnded(sub);
}

BlfService &BlfService::instance() {
  static BlfService _instance;
  return _instance;
}

BlfService::BlfService(QObject *parent) : QObject(parent) {
  g_subscriptionEnded = [](pjsip_evsub *sub) {
    BlfService::instance().subscriptionEnded(sub);
  };
}

// PJSIP thread: the pointer dies with this callback, so forget it right away
void BlfService::subscriptionEnded(pjsip_evsub *sub) {
  QString accountId;
  {
    QMutexLocker lock(&m_mutex);
    accountId = m_subs.key(sub);
    if (accountId.isEmpty())
      return; // we ended it ourselves
    m_subs.remove(accountId);
  }
  qWarning() << "BLF subscription ended for" << accountId;

  // Rejected or expired on the server: try again later, on the GUI thread
  QMetaObject::invokeMethod(
      this,
      [this, accountId] {
        // A QTimer object rather than QTimer::singleShot(): see
        // QtLibcxxCompat.cpp
        auto *retry = new QTimer(this);
        retry->setSingleShot(true);
        connect(retry, &QTimer::timeout, this, [this, accountId, retry] {
          retry->deleteLater();
          QMutexLocker lock(&m_mutex);
          const bool active = m_subs.contains(accountId);
          lock.unlock();
          if (!active && m_wanted.contains(accountId))
            subscribe(accountId, m_wanted[accountId]);
        });
        retry->start(60000);
      },
      Qt::QueuedConnection);
}

void BlfService::unsubscribe(const QString &accountId) {
  m_wanted.remove(accountId);
  pjsip_evsub *sub = nullptr;
  {
    QMutexLocker lock(&m_mutex);
    sub = m_subs.take(accountId);
  }
  if (!sub || pjsua_get_state() != PJSUA_STATE_RUNNING)
    return;
  pjsip_tx_data *tdata = nullptr;
  if (pjsip_evsub_initiate(sub, nullptr, 0, &tdata) == PJ_SUCCESS)
    pjsip_evsub_send_request(sub, tdata);
}

void BlfService::subscribe(const QString &accountId, const QString &listUri) {
  unsubscribe(accountId);
  if (listUri.trimmed().isEmpty())
    return;
  m_wanted[accountId] = listUri;

  auto account = SipService::instance().getAccount(accountId);
  if (!account || !ensurePackage())
    return;
  const pjsua_acc_id acc = account->getId();

  pj_pool_t *pool = pjsua_pool_create("blf", 2048, 1024);
  pjsua_acc_config cfg;
  pjsip_dialog *dlg = nullptr;
  pjsip_evsub *sub = nullptr;
  pj_status_t status = pjsua_acc_get_config(acc, pool, &cfg);

  // Same domain and transport as the account unless the URI says otherwise
  QString target = bareSipUri(listUri);
  if (!target.contains('@'))
    target += accountId.mid(accountId.indexOf('@'));
  const QString registrar = QString::fromUtf8(cfg.reg_uri.ptr, cfg.reg_uri.slen);
  const std::string targetStr =
      ("sip:" + target +
       QRegularExpression(";transport=\\w+").match(registrar).captured())
          .toStdString();
  pj_str_t pjTarget = pj_str(const_cast<char *>(targetStr.c_str()));

  pj_str_t contact;
  if (status == PJ_SUCCESS)
    status = pjsua_acc_create_uac_contact(pool, &contact, acc, &pjTarget);
  if (status == PJ_SUCCESS)
    status = pjsip_dlg_create_uac(pjsip_ua_instance(), &cfg.id, &contact,
                                  &pjTarget, nullptr, &dlg);
  if (status == PJ_SUCCESS) {
    pjsip_dlg_inc_lock(dlg);
    pjsip_auth_clt_set_credentials(&dlg->auth_sess, cfg.cred_count,
                                   cfg.cred_info);

    // Go through the account's outbound proxies, as loose routes
    pjsip_route_hdr routes;
    pj_list_init(&routes);
    const pj_str_t routeName = {(char *)"Route", 5};
    for (unsigned i = 0; i < cfg.proxy_cnt; ++i) {
      QByteArray value(cfg.proxy[i].ptr, cfg.proxy[i].slen);
      if (!value.contains(";lr"))
        value += ";lr";
      value = "<" + value + ">";
      char *buf = static_cast<char *>(pj_pool_alloc(dlg->pool, value.size() + 1));
      pj_memcpy(buf, value.constData(), value.size() + 1);
      void *route =
          pjsip_parse_hdr(dlg->pool, &routeName, buf, value.size(), nullptr);
      if (route)
        pj_list_push_back(&routes, route);
    }
    if (!pj_list_empty(&routes))
      pjsip_dlg_set_route_set(dlg, &routes);

    pjsip_evsub_user callbacks;
    pj_bzero(&callbacks, sizeof(callbacks));
    callbacks.on_evsub_state = &onEvsubState;
    callbacks.on_rx_notify = &onRxNotify;
    status = pjsip_evsub_create_uac(dlg, &callbacks, &STR_DIALOG,
                                    PJSIP_EVSUB_NO_EVENT_ID, &sub);
    if (status == PJ_SUCCESS) {
      {
        // Registered before sending: a rejection can end it immediately
        QMutexLocker lock(&m_mutex);
        m_subs[accountId] = sub;
      }
      pjsip_tx_data *tdata = nullptr;
      status = pjsip_evsub_initiate(sub, nullptr, -1, &tdata);
      if (status == PJ_SUCCESS)
        status = pjsip_evsub_send_request(sub, tdata);
      if (status != PJ_SUCCESS) {
        QMutexLocker lock(&m_mutex);
        if (m_subs.value(accountId) == sub) {
          m_subs.remove(accountId);
          lock.unlock();
          pjsip_evsub_terminate(sub, PJ_FALSE);
        }
      }
    }
    // With no subscription attached, releasing the lock destroys the dialog
    pjsip_dlg_dec_lock(dlg);
  }
  pj_pool_release(pool);

  if (status != PJ_SUCCESS)
    qWarning() << "Could not subscribe to BLF list" << listUri << "status"
               << status;
  else
    qInfo() << "Subscribed to BLF list" << QString::fromStdString(targetStr);
}
