#ifndef HARDWARESERVICE_H
#define HARDWARESERVICE_H

#include <QMap>
#include <QObject>
#include <map>
#include <QStringList>
#include <QVariantMap>
#include <pjsua2.hpp>

class HardwareService : public QObject {
  Q_OBJECT
  Q_PROPERTY(QStringList audioPlaybackDevices READ audioPlaybackDevices NOTIFY
                 devicesChanged)
  Q_PROPERTY(QStringList audioCaptureDevices READ audioCaptureDevices NOTIFY
                 devicesChanged)
  // Android: what a call can be heard through (earpiece, loudspeaker,
  // headset), the one in use first. Empty elsewhere, where Settings chooses.
  Q_PROPERTY(QStringList audioOutputs READ audioOutputs NOTIFY
                 audioOutputsChanged)
  Q_PROPERTY(QStringList videoDevices READ videoDevices NOTIFY devicesChanged)
  Q_PROPERTY(QStringList audioCodecs READ audioCodecs NOTIFY devicesChanged)
  Q_PROPERTY(QStringList videoCodecs READ videoCodecs NOTIFY devicesChanged)
  Q_PROPERTY(int captureIndex READ captureIndex NOTIFY devicesChanged)
  Q_PROPERTY(int playbackIndex READ playbackIndex NOTIFY devicesChanged)
  // The camera used for video calls, as a position in videoDevices
  Q_PROPERTY(int videoIndex READ videoIndex NOTIFY devicesChanged)
  Q_PROPERTY(bool videoPreviewActive READ videoPreviewActive NOTIFY
                 videoPreviewChanged)
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
  int captureIndex() const { return m_captureIndex; }
  int playbackIndex() const { return m_playbackIndex; }
  int videoIndex() const { return m_videoIndex; }
  bool videoPreviewActive() const { return m_videoPreview != nullptr; }
  // Where the running preview's pictures come from in PJSIP's video
  // bridge, or -1
  int previewConfPort() const;
  Q_INVOKABLE void setVideoDevice(int index);
  // The picture sent in video calls: "640x480" and frames per second. An
  // empty size or 0 keeps the codec's own. Used from the next call on.
  Q_INVOKABLE QString videoSize() const;
  Q_INVOKABLE int videoFps() const;
  Q_INVOKABLE void setVideoFormat(const QString &size, int fps);
  void applyVideoFormat();
  // PJSIP's id of the chosen camera, or its "default camera" id
  int selectedVideoDevice() const;

  Q_INVOKABLE void refreshDevices();
  QStringList audioOutputs() const { return m_audioOutputs; }
  Q_INVOKABLE void setAudioOutput(const QString &name);
  Q_INVOKABLE void refreshAudioOutputs();
  // Android: puts the phone in or out of call audio, on the loudspeaker
  void useLoudspeaker(bool on);
  Q_INVOKABLE void setAudioDevice(int captureIdx, int playbackIdx);

  Q_INVOKABLE bool isCodecEnabled(const QString &codecId) const;
  Q_INVOKABLE void setCodecEnabled(const QString &codecId, bool enabled);
  Q_INVOKABLE bool isVideoCodecEnabled(const QString &codecId) const;
  Q_INVOKABLE void setVideoCodecEnabled(const QString &codecId, bool enabled);
  void applyCodecPriorities();

  float speakerVolume() const;
  void setSpeakerVolume(float volume);

  float micVolume() const;
  void setMicVolume(float volume);

  bool echoCancellation() const;
  void setEchoCancellation(bool enabled);

  // Devices, volumes and echo cancellation apply at once but are only kept
  // across restarts when saved; restoring goes back to what was saved
  Q_INVOKABLE void saveSettings();
  Q_INVOKABLE void restoreSettings();

  Q_INVOKABLE void startAudioTest(int captureIdx, int playbackIdx);
  Q_INVOKABLE void stopAudioTest();

  Q_INVOKABLE void startVideoPreview(int devIdx);
  Q_INVOKABLE void stopVideoPreview();

signals:
  void devicesChanged();
  void audioOutputsChanged();
  void volumeChanged();
  void echoCancellationChanged();
  void videoPreviewChanged();

private:
  explicit HardwareService(QObject *parent = nullptr);
  ~HardwareService();
  pj::Endpoint *m_endpoint;
  pj::VideoPreview *m_videoPreview;
  int m_previewDevice = -1;
  bool m_echoCancellation = true; // pjsua enables EC by default
  int m_captureIndex = 0; // positions in the lists exposed to QML
  int m_playbackIndex = 0;
  int m_videoIndex = 0;
  mutable bool m_speakerVolumeUnsupported = false;
  mutable bool m_micVolumeUnsupported = false;
  QString m_savedVideoDevice; // what the accounts were last built with
  QStringList m_audioPlaybackDevices;
  QStringList m_audioOutputs;
  QStringList m_audioCaptureDevices;
  QList<int> m_playbackDeviceIndices;
  QList<int> m_captureDeviceIndices;
  QStringList m_videoDevices;
  QList<int> m_videoDeviceIndices;
  QStringList m_audioCodecs;
  QStringList m_videoCodecs;
  // What each video codec was set to before the user chose a format
  std::map<std::string, pj::VidCodecParam> m_videoCodecDefaults;
  QStringList m_disabledCodecs;
  QStringList m_disabledVideoCodecs;
  // The video codecs' own order of preference, to give back on enabling
  QMap<QString, int> m_videoCodecPriorities;
};

#endif // HARDWARESERVICE_H
