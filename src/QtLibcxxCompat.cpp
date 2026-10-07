// The portable Linux build (`make static`) compiles with zig's libc++, while
// the official Qt binaries for Linux are built with libstdc++. The two give std::
// types different symbol names, so a Qt function with one in its signature
// cannot be found at link time. Our code avoids those functions, except for
// this one, which QString::toStdString() needs; it is supplied here instead.
// The Windows, macOS and Android Qt kits are built with libc++ and need none
// of this.
#include <QByteArray>
#include <string>

#if defined(_LIBCPP_VERSION) && defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
std::string QByteArray::toStdString() const {
  return std::string(constData(), size_t(size()));
}
#endif
