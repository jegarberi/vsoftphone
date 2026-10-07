#ifndef SIPURI_H
#define SIPURI_H

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

#endif // SIPURI_H
