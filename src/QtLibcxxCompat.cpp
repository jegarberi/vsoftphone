// The portable build (`make static`) compiles with zig's libc++, while the
// official Qt binaries it links are built with libstdc++. The two give std::
// types different symbol names, so a Qt function with one in its signature
// cannot be found at link time. Our code avoids those functions, except for
// this one, which QString::toStdString() needs; it is supplied here instead.
#include <QByteArray>
#include <string>

#ifdef _LIBCPP_VERSION
std::string QByteArray::toStdString() const {
  return std::string(constData(), size_t(size()));
}
#endif
