#ifndef VIDEOSINK_H
#define VIDEOSINK_H

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQuickPaintedItem>
#include <QSize>
#include <atomic>
#include <pjmedia/port.h>

// Receives the pictures of the video being shown, so that the interface can
// draw them itself instead of PJSIP opening a window of its own: the other
// side of a video call, or else the camera preview of Settings.
//
// ponytail: one picture for the whole app. Two video calls at once both show
// the newer one; give each call its own sink if that ever matters.
class VideoSink : public QObject {
  Q_OBJECT
public:
  // The other side of a video call, or else the Settings camera preview
  static VideoSink &instance();
  // Our own camera, as the other side of a video call gets it
  static VideoSink &self();

  // Looks for the video to show and follows it; cheap when nothing changed.
  // GUI thread.
  void refresh();
  QImage frame() const;
  // Pictures received since the app started
  int frameCount() const { return m_frames; }
  // PJSIP's video thread
  void receive(const void *pixels, int size);

signals:
  void frameChanged();

private:
  explicit VideoSink(bool self);
  void detach();
  int findSource() const;
  static pj_status_t putFrame(pjmedia_port *port, pjmedia_frame *frame);
  static pj_status_t destroyPort(pjmedia_port *port);
  const bool m_self;

  struct Port;
  Port *m_port = nullptr;
  int m_slot = -1;   // ours in PJSIP's video bridge
  int m_source = -1; // the one we are fed from
  QSize m_size;
  mutable QMutex m_mutex;
  QImage m_frame;
  std::atomic<int> m_frames{0};
  std::atomic<bool> m_announced{false};
};

// Draws those pictures, whole and in their own proportions
class VideoView : public QQuickPaintedItem {
  Q_OBJECT
  // Our own camera instead of the other side's picture
  Q_PROPERTY(bool self MEMBER m_self NOTIFY selfChanged)
  // False while there is nothing to draw
  Q_PROPERTY(bool live READ live NOTIFY liveChanged)
public:
  explicit VideoView(QQuickItem *parent = nullptr);
  void paint(QPainter *painter) override;
  bool live() const { return m_live; }

signals:
  void selfChanged();
  void liveChanged();

private:
  VideoSink &sink() const;
  bool m_self = false;
  bool m_live = false;
};

#endif // VIDEOSINK_H
