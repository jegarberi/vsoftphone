#ifndef VIDEOSINK_H
#define VIDEOSINK_H

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQuickPaintedItem>
#include <QSize>
#include <atomic>

// Receives the pictures of the video being shown, so that the interface can
// draw them itself instead of PJSIP opening a window of its own: the other
// side of a video call, or else the camera preview of Settings.
//
// ponytail: one picture for the whole app. Two video calls at once both show
// the newer one; give each call its own sink if that ever matters.
class VideoSink : public QObject {
  Q_OBJECT
public:
  static VideoSink &instance();

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
  explicit VideoSink(QObject *parent = nullptr);
  void detach();

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
public:
  explicit VideoView(QQuickItem *parent = nullptr);
  void paint(QPainter *painter) override;
};

#endif // VIDEOSINK_H
