#ifndef SIPURI_H
#define SIPURI_H

#include <QRegularExpression>
#include <QString>

// "Name" <sip:user@host;transport=tcp>  ->  user@host
// sip:1001                              ->  1001
inline QString bareSipUri(QString uri) {
  const int lt = uri.indexOf('<');
  if (lt >= 0) {
    const int gt = uri.indexOf('>', lt);
    uri = uri.mid(lt + 1, gt < 0 ? -1 : gt - lt - 1);
  }
  uri = uri.trimmed();
  if (uri.startsWith("sips:", Qt::CaseInsensitive))
    uri = uri.mid(5);
  else if (uri.startsWith("sip:", Qt::CaseInsensitive))
    uri = uri.mid(4);
  const int semi = uri.indexOf(';');
  if (semi >= 0)
    uri.truncate(semi);
  return uri;
}

// True when the host part of a bare URI is an IP literal, optionally with a
// port: "10.0.0.5", "bob@10.0.0.5:5080", "[fe80::1]:5060". Deliberately strict:
// an extension such as "1001" must not be taken for an address.
inline bool hasIpHost(const QString &bareUri) {
  static const QRegularExpression ip(
      R"(^(\d{1,3}(\.\d{1,3}){3}|\[[0-9A-Fa-f:.]+\])(:\d+)?$)");
  return ip.match(bareUri.section('@', -1)).hasMatch();
}

#endif // SIPURI_H
