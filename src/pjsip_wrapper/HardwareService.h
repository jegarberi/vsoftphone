#ifndef HARDWARESERVICE_H
#define HARDWARESERVICE_H

#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <pjsua2.hpp>

class HardwareService : public QObject {
  Q_OBJECT
  Q_PROPERTY(QStringList audioPlaybackDevices READ audioPlaybackDevices NOTIFY
                 devicesChanged)
  Q_PROPERTY(QStringList audioCaptureDevices READ audioCaptureDevices NOTIFY
                 devicesChanged)
  Q_PROPERTY(QStringList videoDevices READ videoDevices NOTIFY devicesChanged)
  Q_PROPERTY(QStringList audioCodecs READ audioCodecs NOTIFY devicesChanged)
  Q_PROPERTY(QStringList videoCodecs READ videoCodecs NOTIFY devicesChanged)
  Q_PROPERTY(float speakerVolume READ speakerVolume WRITE setSpeakerVolume
                 NOTIFY volumeChanged)
  Q_PROPERTY(
      float micVolume READ micVolume WRITE setMicVolume NOTIFY volumeChanged)
  Q_PROPERTY(bool echoCancellation READ echoCancellation WRITE
                 setEchoCancellation NOTIFY echoCancellationChanged)

public:
  static HardwareService &instance();

  void init(pj::Endpoint *endpoint);

  QStringList audioPlaybackDevices() const { return m_audioPlaybackDevices; }
  QStringList audioCaptureDevices() const { return m_audioCaptureDevices; }
  QStringList videoDevices() const { return m_videoDevices; }
  QStringList audioCodecs() const { return m_audioCodecs; }
  QStringList videoCodecs() const { return m_videoCodecs; }

  Q_INVOKABLE void refreshDevices();
  Q_INVOKABLE void setAudioDevice(int captureIdx, int playbackIdx);
  Q_INVOKABLE void setVideoDevice(int devIdx);

  float speakerVolume() const;
  void setSpeakerVolume(float volume);

  float micVolume() const;
  void setMicVolume(float volume);

  bool echoCancellation() const;
  void setEchoCancellation(bool enabled);

  Q_INVOKABLE void startAudioTest(int captureIdx, int playbackIdx);
  Q_INVOKABLE void stopAudioTest();

  Q_INVOKABLE void startVideoPreview(int devIdx);
  Q_INVOKABLE void stopVideoPreview();

signals:
  void devicesChanged();
  void volumeChanged();
  void echoCancellationChanged();

private:
  explicit HardwareService(QObject *parent = nullptr);
  pj::Endpoint *m_endpoint;
  pj::VideoPreview *m_videoPreview;
  bool m_echoCancellation = true; // pjsua enables EC by default
  QStringList m_audioPlaybackDevices;
  QStringList m_audioCaptureDevices;
  QList<int> m_playbackDeviceIndices;
  QList<int> m_captureDeviceIndices;
  QStringList m_videoDevices;
  QStringList m_audioCodecs;
  QStringList m_videoCodecs;
};

#endif // HARDWARESERVICE_H
