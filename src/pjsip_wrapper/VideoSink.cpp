#include "VideoSink.h"
#include "CallService.h"
#include "HardwareService.h"
#include <QPainter>
#include <QTimer>
#include <pjsua-lib/pjsua.h>

// A sink in PJSIP's video bridge. The bridge converts whatever it is
// connected to into this port's format and hands over one picture at a time.
struct VideoSink::Port {
  pjmedia_port base; // base.port_data.pdata is the pool it lives in
  VideoSink *owner;
};

pj_status_t VideoSink::putFrame(pjmedia_port *port, pjmedia_frame *frame) {
  if (frame->type == PJMEDIA_FRAME_TYPE_VIDEO && frame->buf && frame->size)
    reinterpret_cast<Port *>(port)->owner->receive(frame->buf,
                                                   int(frame->size));
  return PJ_SUCCESS;
}

// The bridge lets go of a port some time after it is asked to
pj_status_t VideoSink::destroyPort(pjmedia_port *port) {
  pj_pool_release(static_cast<pj_pool_t *>(port->port_data.pdata));
  return PJ_SUCCESS;
}

VideoSink &VideoSink::instance() {
  static VideoSink sink(false);
  return sink;
}

VideoSink &VideoSink::self() {
  static VideoSink sink(true);
  return sink;
}

VideoSink::VideoSink(bool self) : m_self(self) {
  // ponytail: polled. The size of a call's picture is only known once the
  // first one is decoded, and can change later; PJSIP's media events would
  // say exactly when.
  auto *follow = new QTimer(this);
  connect(follow, &QTimer::timeout, this, &VideoSink::refresh);
  follow->start(500);
  // Sooner than the next poll when we are told something changed
  connect(&HardwareService::instance(), &HardwareService::videoPreviewChanged,
          this, &VideoSink::refresh);
  connect(&CallService::instance(), &CallService::callMediaStateChanged, this,
          &VideoSink::refresh);
}

QImage VideoSink::frame() const {
  QMutexLocker lock(&m_mutex);
  return m_frame;
}

void VideoSink::receive(const void *pixels, int size) {
  {
    QMutexLocker lock(&m_mutex);
    if (m_frame.isNull() || size < m_frame.sizeInBytes())
      return;
    memcpy(m_frame.bits(), pixels, size_t(m_frame.sizeInBytes()));
  }
  ++m_frames;
  // One repaint request at a time, however fast the pictures arrive
  if (!m_announced.exchange(true))
    QMetaObject::invokeMethod(
        this,
        [this] {
          m_announced = false;
          emit frameChanged();
        },
        Qt::QueuedConnection);
}

void VideoSink::detach() {
#if PJSUA_HAS_VIDEO
  if (m_slot >= 0 && pjsua_get_state() == PJSUA_STATE_RUNNING)
    pjsua_vid_conf_remove_port(m_slot);
  if (m_port)
    pjmedia_port_dec_ref(&m_port->base);
#endif
  m_port = nullptr;
  m_slot = m_source = -1;
  m_size = QSize();
  {
    QMutexLocker lock(&m_mutex);
    m_frame = QImage();
  }
  emit frameChanged();
}

// Where in PJSIP's video bridge our pictures come from now, or -1
int VideoSink::findSource() const {
  int source = -1;
#if PJSUA_HAS_VIDEO
  // A call's incoming video comes before the preview
  if (!m_self)
    source = HardwareService::instance().previewConfPort();
  pjsua_call_id calls[PJSUA_MAX_CALLS];
  unsigned count = PJ_ARRAY_SIZE(calls);
  pjsua_enum_calls(calls, &count);
  for (unsigned i = 0; i < count; ++i) {
    pjsua_call_info info;
    if (pjsua_call_get_info(calls[i], &info) != PJ_SUCCESS)
      continue;
    for (unsigned m = 0; m < info.media_cnt; ++m) {
      if (info.media[m].type != PJMEDIA_TYPE_VIDEO ||
          info.media[m].status != PJSUA_CALL_MEDIA_ACTIVE)
        continue;
      if (!m_self) {
        if (info.media[m].stream.vid.dec_slot >= 0)
          source = info.media[m].stream.vid.dec_slot;
        continue;
      }
      // Our camera is whatever feeds this call's encoder. (PJSIP has no
      // call for it: it only answers for a preview the app started.)
      pjsua_conf_port_id ports[PJSUA_MAX_CONF_PORTS];
      unsigned portCount = PJ_ARRAY_SIZE(ports);
      pjsua_vid_conf_enum_ports(ports, &portCount);
      for (unsigned p = 0; p < portCount; ++p) {
        pjsua_vid_conf_port_info feeder;
        if (pjsua_vid_conf_get_port_info(ports[p], &feeder) != PJ_SUCCESS)
          continue;
        for (unsigned l = 0; l < feeder.listener_cnt; ++l) {
          if (feeder.listeners[l] == info.media[m].stream.vid.enc_slot)
            source = ports[p];
        }
      }
    }
  }
#endif
  return source;
}

void VideoSink::refresh() {
#if PJSUA_HAS_VIDEO
  if (pjsua_get_state() != PJSUA_STATE_RUNNING)
    return;
  int source = findSource();
  QSize size;
  pjsua_vid_conf_port_info info;
  if (source >= 0 && pjsua_vid_conf_get_port_info(source, &info) == PJ_SUCCESS) {
    if (const auto *video =
            pjmedia_format_get_video_format_detail(&info.format, PJ_TRUE))
      size = QSize(int(video->size.w), int(video->size.h));
  }
  if (size.isEmpty())
    source = -1;
  if (source == m_source && size == m_size)
    return;

  detach();
  if (source < 0)
    return;

  // Same size as the source, so that the bridge neither crops nor scales;
  // BGRA is what a QImage of Format_RGB32 holds
  pj_pool_t *pool = pjsua_pool_create("videosink", 1024, 1024);
  auto *port = PJ_POOL_ZALLOC_T(pool, Port);
  port->base.port_data.pdata = pool;
  port->owner = this;
  const auto *video =
      pjmedia_format_get_video_format_detail(&info.format, PJ_TRUE);
  pjmedia_format format;
  pjmedia_format_init_video(&format, PJMEDIA_FORMAT_BGRA, size.width(),
                            size.height(), video->fps.num, video->fps.denum);
  const pj_str_t name = pj_str(const_cast<char *>("videosink"));
  pjmedia_port_info_init2(&port->base.info, &name,
                          PJMEDIA_SIG_CLASS_PORT_VID('Q', 'T'),
                          PJMEDIA_DIR_ENCODING, &format);
  port->base.put_frame = &putFrame;
  port->base.on_destroy = &destroyPort;
  pjmedia_port_init_grp_lock(&port->base, pool, nullptr);
  {
    QMutexLocker lock(&m_mutex);
    m_frame = QImage(size, QImage::Format_RGB32);
    m_frame.fill(Qt::black);
  }
  pjsua_conf_port_id slot = PJSUA_INVALID_ID;
  if (pjsua_vid_conf_add_port(pool, &port->base, nullptr, &slot) != PJ_SUCCESS ||
      pjsua_vid_conf_connect(source, slot, nullptr) != PJ_SUCCESS) {
    qWarning() << "VideoSink: cannot attach to the video";
    if (slot != PJSUA_INVALID_ID)
      pjsua_vid_conf_remove_port(slot);
    pjmedia_port_dec_ref(&port->base);
    QMutexLocker lock(&m_mutex);
    m_frame = QImage();
    return;
  }
  m_port = port;
  m_slot = slot;
  m_source = source;
  m_size = size;
#endif
}

VideoView::VideoView(QQuickItem *parent) : QQuickPaintedItem(parent) {
  const auto follow = [this] {
    const bool live = !sink().frame().isNull();
    if (live != m_live) {
      m_live = live;
      emit liveChanged();
    }
    update();
  };
  connect(&VideoSink::instance(), &VideoSink::frameChanged, this, follow);
  connect(&VideoSink::self(), &VideoSink::frameChanged, this, follow);
  connect(this, &VideoView::selfChanged, this, follow);
}

VideoSink &VideoView::sink() const {
  return m_self ? VideoSink::self() : VideoSink::instance();
}

void VideoView::paint(QPainter *painter) {
  const QImage frame = sink().frame();
  if (frame.isNull())
    return;
  const QSizeF fitted =
      QSizeF(frame.size()).scaled(size(), Qt::KeepAspectRatio);
  painter->setRenderHint(QPainter::SmoothPixmapTransform);
  painter->drawImage(QRectF((width() - fitted.width()) / 2,
                            (height() - fitted.height()) / 2, fitted.width(),
                            fitted.height()),
                     frame);
}
