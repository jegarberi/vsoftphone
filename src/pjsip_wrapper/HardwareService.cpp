#include "HardwareService.h"
#include "SipService.h"
#include <QDebug>
#include <QSettings>
#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#endif
#include <pjsua2/endpoint.hpp>
#include <pjsua2/media.hpp>
#include <pjsua-lib/pjsua.h>

using namespace pj;

HardwareService &HardwareService::instance() {
  static HardwareService _instance;
  return _instance;
}

HardwareService::HardwareService(QObject *parent)
    : QObject(parent), m_endpoint(nullptr), m_videoPreview(nullptr) {}

// PJSIP media thread. Closing the preview's window (it is a window of its
// own, drawn by PJSIP) has to end the preview: release the camera and let the
// button say "Preview" again.
static pj_status_t onMediaEvent(pjmedia_event *event, void *) {
  if ((event->type == PJMEDIA_EVENT_WND_CLOSING ||
       event->type == PJMEDIA_EVENT_WND_CLOSED) &&
      pjsua_get_state() == PJSUA_STATE_RUNNING) {
    QMetaObject::invokeMethod(
        &HardwareService::instance(),
        [] { HardwareService::instance().stopVideoPreview(); },
        Qt::QueuedConnection);
  }
  return PJ_SUCCESS;
}

HardwareService::~HardwareService() {
  if (m_endpoint && pjsua_get_state() == PJSUA_STATE_RUNNING)
    pjmedia_event_unsubscribe(nullptr, &onMediaEvent, nullptr, nullptr);
}

void HardwareService::init(pj::Endpoint *endpoint) {
  if (!m_endpoint) // once: every video window reports through this
    pjmedia_event_subscribe(nullptr, &onMediaEvent, nullptr, nullptr);
  m_endpoint = endpoint;
  refreshDevices();
  restoreSettings();
  applyVideoFormat();
}

// Devices are matched by name: their indices change as hardware comes and goes
void HardwareService::restoreSettings() {
  QSettings settings;
  int capture = m_audioCaptureDevices.indexOf(
      settings.value("audio/capture").toString());
  int playback = m_audioPlaybackDevices.indexOf(
      settings.value("audio/playback").toString());
  if (capture >= 0 || playback >= 0)
    setAudioDevice(capture, playback);
  m_savedVideoDevice = settings.value("video/capture").toString();
  m_videoIndex = qMax(0, m_videoDevices.indexOf(m_savedVideoDevice));
  emit devicesChanged();
  // Volumes are not restored: they are the device's own mixer levels, and
  // forcing ours at startup would override what the user set elsewhere
  if (settings.contains("audio/echoCancellation"))
    setEchoCancellation(settings.value("audio/echoCancellation").toBool());
}

void HardwareService::saveSettings() {
  QSettings settings;
  settings.setValue("audio/capture",
                    m_audioCaptureDevices.value(m_captureIndex));
  settings.setValue("audio/playback",
                    m_audioPlaybackDevices.value(m_playbackIndex));
  settings.setValue("audio/echoCancellation", m_echoCancellation);

  // The camera is part of each account's configuration
  const QString camera = m_videoDevices.value(m_videoIndex);
  settings.setValue("video/capture", camera);
  if (camera != m_savedVideoDevice) {
    m_savedVideoDevice = camera;
    SipService::instance().markAccountsChanged();
  }
}

void HardwareService::setVideoDevice(int index) {
  if (index < 0 || index >= m_videoDevices.size() || index == m_videoIndex)
    return;
  m_videoIndex = index;
  emit devicesChanged();
}

int HardwareService::selectedVideoDevice() const {
  return m_videoDeviceIndices.value(m_videoIndex,
                                    PJMEDIA_VID_DEFAULT_CAPTURE_DEV);
}

void HardwareService::refreshDevices() {
  if (!m_endpoint)
    return;

  m_audioPlaybackDevices.clear();
  m_audioCaptureDevices.clear();
  m_playbackDeviceIndices.clear();
  m_captureDeviceIndices.clear();
  m_videoDevices.clear();
  m_videoDeviceIndices.clear();
  m_audioCodecs.clear();
  m_videoCodecs.clear();

  try {
    // Audio Devices
    AudDevManager &adm = m_endpoint->audDevManager();
    AudioDevInfoVector2 audioDevs = adm.enumDev2();
    qDebug() << "HardwareService: Found" << audioDevs.size() << "audio devices";

    // First entry follows the system's default device (PJSIP's -1 and -2)
    if (!audioDevs.empty()) {
      m_audioCaptureDevices.append("System default");
      m_captureDeviceIndices.append(PJMEDIA_AUD_DEFAULT_CAPTURE_DEV);
      m_audioPlaybackDevices.append("System default");
      m_playbackDeviceIndices.append(PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV);
    }

    for (unsigned i = 0; i < audioDevs.size(); ++i) {
      const AudioDevInfo &info = audioDevs[i];
      QString name = QString::fromStdString(info.name);

      if (info.inputCount > 0) {
        m_audioCaptureDevices.append(name);
        m_captureDeviceIndices.append(i);
      }
      if (info.outputCount > 0) {
        m_audioPlaybackDevices.append(name);
        m_playbackDeviceIndices.append(i);
      }
    }

    // If no devices found (e.g. ALSA failed), add Null Audio Device option
    if (m_audioPlaybackDevices.isEmpty() && m_audioCaptureDevices.isEmpty()) {
      qDebug() << "No physical devices found, adding Null Audio Device";
      m_audioCaptureDevices.append("Null Audio Device");
      m_captureDeviceIndices.append(-99);
      m_audioPlaybackDevices.append("Null Audio Device");
      m_playbackDeviceIndices.append(-99);
    }

    // Audio Codecs
    const CodecInfoVector2 &aCodecs = m_endpoint->codecEnum2();
    for (unsigned i = 0; i < aCodecs.size(); ++i) {
      m_audioCodecs.append(QString::fromStdString(aCodecs[i].codecId));
    }

  } catch (Error &err) {
    qWarning() << "Error refreshing devices:"
               << QString::fromStdString(err.info());
  }

  // Video is separate: a PJSIP built without it must not cost us the lists
  // above (PJSUA_HAS_VIDEO is 0 in the portable build)
  try {
    // Video Devices
    VidDevManager &vdm = m_endpoint->vidDevManager();
    VideoDevInfoVector2 videoDevs = vdm.enumDev2();
    qDebug() << "HardwareService: Found" << videoDevs.size() << "video devices";
    // A camera is a capture device that says which picture formats it has;
    // one that reports none cannot be opened
    auto usable = [](const VideoDevInfo &info) {
      return info.dir != PJMEDIA_DIR_RENDER && !info.fmt.empty();
    };
    bool anyCamera = false;
    for (unsigned i = 0; i < videoDevs.size(); ++i)
      anyCamera = anyCamera || usable(videoDevs[i]);
    if (anyCamera) {
      m_videoDevices.append("System default");
      m_videoDeviceIndices.append(PJMEDIA_VID_DEFAULT_CAPTURE_DEV);
    }
    for (unsigned i = 0; i < videoDevs.size(); ++i) {
      const VideoDevInfo &info = videoDevs[i];
      QString name = QString::fromStdString(info.name);
      QString driver = QString::fromStdString(info.driver);
      QString friendlyName =
          driver.isEmpty() ? name : QString("%1 (%2)").arg(name, driver);
      if (!usable(info))
        continue;
      m_videoDevices.append(friendlyName);
      m_videoDeviceIndices.append(i);
      qDebug() << " - Video Device:" << friendlyName;
    }

#ifdef Q_OS_ANDROID
    // A phone's camera sensor lies on its side; the phone is held upright
    for (int index : std::as_const(m_videoDeviceIndices)) {
      if (index >= 0)
        vdm.setCaptureOrient(index, PJMEDIA_ORIENT_ROTATE_270DEG, true);
    }
#endif

    // Video Codecs
    const CodecInfoVector2 &vCodecs = m_endpoint->videoCodecEnum2();
    qDebug() << "HardwareService: Found" << vCodecs.size() << "video codecs";
    for (unsigned i = 0; i < vCodecs.size(); ++i) {
      m_videoCodecs.append(QString::fromStdString(vCodecs[i].codecId));
      qDebug() << " - Video Codec:"
               << QString::fromStdString(vCodecs[i].codecId);
    }

  } catch (Error &err) {
    qInfo() << "No video support:" << QString::fromStdString(err.info());
  }

  emit devicesChanged();
}

void HardwareService::setAudioDevice(int captureIdx, int playbackIdx) {
  if (!m_endpoint)
    return;
  try {
    if (captureIdx >= 0 && captureIdx < m_captureDeviceIndices.size()) {
      int pjCaptureIdx = m_captureDeviceIndices[captureIdx];
      if (pjCaptureIdx == -99) {
        m_endpoint->audDevManager().setNullDev();
      } else {
        m_endpoint->audDevManager().setCaptureDev(pjCaptureIdx);
      }
      m_captureIndex = captureIdx;
    }
    if (playbackIdx >= 0 && playbackIdx < m_playbackDeviceIndices.size()) {
      int pjPlaybackIdx = m_playbackDeviceIndices[playbackIdx];
      if (pjPlaybackIdx == -99) {
        m_endpoint->audDevManager().setNullDev();
      } else {
        m_endpoint->audDevManager().setPlaybackDev(pjPlaybackIdx);
      }
      m_playbackIndex = playbackIdx;
    }
  } catch (Error &err) {
    qWarning() << "Error setting audio device:"
               << QString::fromStdString(err.info());
  }
}

// Enabled codecs are ranked so the SDP offer order is stable between runs
static int codecPriority(const QString &codecId) {
  static const QStringList preference{"PCMU", "PCMA", "G722", "opus"};
  int rank = 0;
  for (int i = 0; i < preference.size(); ++i) {
    if (codecId.contains(preference[i], Qt::CaseInsensitive))
      rank = i + 1;
  }
  return 128 + rank;
}

void HardwareService::applyCodecPriorities() {
  if (!m_endpoint)
    return;

  QSettings settings;
  const bool configured = settings.contains("audio/disabledCodecs");
  m_disabledCodecs = settings.value("audio/disabledCodecs").toStringList();
  try {
    const CodecInfoVector2 codecs = m_endpoint->codecEnum2();
    for (unsigned i = 0; i < codecs.size(); ++i) {
      const QString id = QString::fromStdString(codecs[i].codecId);
      // First run: only the common codecs, to keep the SDP small
      if (!configured && codecPriority(id) == 128 &&
          !id.contains("telephone-event"))
        m_disabledCodecs.append(id);
      m_endpoint->codecSetPriority(
          codecs[i].codecId,
          m_disabledCodecs.contains(id) ? 0 : codecPriority(id));
    }
  } catch (Error &err) {
    qWarning() << "Error configuring codecs:"
               << QString::fromStdString(err.info());
  }

  // Video codecs keep PJSIP's own order; the ones turned off are left out.
  // Throws where PJSIP was built without video.
  m_disabledVideoCodecs = settings.value("video/disabledCodecs").toStringList();
  try {
    const CodecInfoVector2 codecs = m_endpoint->videoCodecEnum2();
    for (const CodecInfo &codec : codecs) {
      const QString id = QString::fromStdString(codec.codecId);
      if (!m_videoCodecPriorities.contains(id))
        m_videoCodecPriorities[id] = codec.priority ? codec.priority : 128;
      if (m_disabledVideoCodecs.contains(id))
        m_endpoint->videoCodecSetPriority(codec.codecId, 0);
    }
  } catch (Error &) {
  }
}

QString HardwareService::videoSize() const {
  return QSettings().value("video/size").toString();
}

int HardwareService::videoFps() const {
  return QSettings().value("video/fps", 0).toInt();
}

void HardwareService::setVideoFormat(const QString &size, int fps) {
  QSettings().setValue("video/size", size);
  QSettings().setValue("video/fps", fps);
  applyVideoFormat();
}

// Sets every video codec to send the chosen picture size and frame rate. A
// smaller, slower picture is what helps a call that lags.
void HardwareService::applyVideoFormat() {
  if (!m_endpoint)
    return;
  const QStringList size = videoSize().split('x');
  const int width = size.value(0).toInt(), height = size.value(1).toInt();
  const int fps = videoFps();
  try {
    const CodecInfoVector2 codecs = m_endpoint->videoCodecEnum2();
    for (const CodecInfo &codec : codecs) {
      auto original = m_videoCodecDefaults.find(codec.codecId);
      if (original == m_videoCodecDefaults.end())
        original = m_videoCodecDefaults
                       .emplace(codec.codecId,
                                m_endpoint->getVideoCodecParam(codec.codecId))
                       .first;
      VidCodecParam param = original->second;
      if (width > 0 && height > 0) {
        param.encFmt.width = width;
        param.encFmt.height = height;
      }
      if (fps > 0) {
        param.encFmt.fpsNum = fps;
        param.encFmt.fpsDenum = 1;
      }
      if ((width > 0 && height > 0) || fps > 0) {
        // About a tenth of a bit per pixel, which is what the codecs' own
        // settings work out to
        const double bps = 0.1 * param.encFmt.width * param.encFmt.height *
                           param.encFmt.fpsNum / param.encFmt.fpsDenum;
        param.encFmt.avgBps = qBound(96000, int(bps), 4000000);
        param.encFmt.maxBps = param.encFmt.avgBps * 3 / 2;
      }
      // H.264 states the largest picture it takes as a "level"; the default
      // (3.0) stops below 720p, and the other side would not send more
      // than it is told we accept
      if (width * height > 720 * 576) {
        for (CodecFmtp &fmtp : param.decFmtp) {
          if (fmtp.name == "profile-level-id" && fmtp.val.size() == 6)
            fmtp.val.replace(4, 2, "1f");
        }
      }
      m_endpoint->setVideoCodecParam(codec.codecId, param);
    }
  } catch (Error &err) {
    qWarning() << "Error setting the video format:"
               << QString::fromStdString(err.info());
  }
}

bool HardwareService::isVideoCodecEnabled(const QString &codecId) const {
  return !m_disabledVideoCodecs.contains(codecId);
}

void HardwareService::setVideoCodecEnabled(const QString &codecId,
                                           bool enabled) {
  if (!m_endpoint || enabled == isVideoCodecEnabled(codecId))
    return;
  try {
    m_endpoint->videoCodecSetPriority(
        codecId.toStdString(),
        enabled ? m_videoCodecPriorities.value(codecId, 128) : 0);
    if (enabled)
      m_disabledVideoCodecs.removeAll(codecId);
    else
      m_disabledVideoCodecs.append(codecId);
    QSettings().setValue("video/disabledCodecs", m_disabledVideoCodecs);
  } catch (Error &err) {
    qWarning() << "Error changing codec:" << QString::fromStdString(err.info());
  }
}

bool HardwareService::isCodecEnabled(const QString &codecId) const {
  return !m_disabledCodecs.contains(codecId);
}

void HardwareService::setCodecEnabled(const QString &codecId, bool enabled) {
  if (!m_endpoint || enabled == isCodecEnabled(codecId))
    return;
  try {
    m_endpoint->codecSetPriority(codecId.toStdString(),
                                 enabled ? codecPriority(codecId) : 0);
    if (enabled)
      m_disabledCodecs.removeAll(codecId);
    else
      m_disabledCodecs.append(codecId);
    QSettings().setValue("audio/disabledCodecs", m_disabledCodecs);
  } catch (Error &err) {
    qWarning() << "Error changing codec:" << QString::fromStdString(err.info());
  }
}

#ifdef Q_OS_ANDROID
static const char *phoneService = "io/github/jegarberi/vsoftphone/PhoneService";
#endif

void HardwareService::setAudioOutput(const QString &name) {
#ifdef Q_OS_ANDROID
  QJniObject::callStaticMethod<void>(
      phoneService, "setAudioOutput",
      "(Landroid/content/Context;Ljava/lang/String;)V",
      QNativeInterface::QAndroidApplication::context(),
      QJniObject::fromString(name).object());
  refreshAudioOutputs();
#else
  Q_UNUSED(name);
#endif
}

void HardwareService::refreshAudioOutputs() {
#ifdef Q_OS_ANDROID
  QStringList outputs;
  const QJniObject names = QJniObject::callStaticObjectMethod(
      phoneService, "audioOutputs",
      "(Landroid/content/Context;)[Ljava/lang/String;",
      QNativeInterface::QAndroidApplication::context());
  QJniEnvironment env;
  const auto array = names.object<jobjectArray>();
  for (jsize i = 0; array && i < env->GetArrayLength(array); ++i)
    outputs.append(
        QJniObject::fromLocalRef(env->GetObjectArrayElement(array, i))
            .toString());
  if (outputs == m_audioOutputs)
    return;
  m_audioOutputs = outputs;
  emit audioOutputsChanged();
#endif
}

// Android plays call audio through the earpiece, where a test held at
// arm's length is not heard: the test uses the loudspeaker
void HardwareService::useLoudspeaker(bool on) {
#ifdef Q_OS_ANDROID
  QJniObject::callStaticMethod<void>(
      phoneService, "inCall", "(Landroid/content/Context;Z)V",
      QNativeInterface::QAndroidApplication::context(), jboolean(on));
  if (on)
    setAudioOutput("Loudspeaker");
#else
  Q_UNUSED(on);
#endif
}

void HardwareService::startAudioTest(int captureIdx, int playbackIdx) {
  if (!m_endpoint)
    return;
  try {
    // Set the devices first
    setAudioDevice(captureIdx, playbackIdx);

    // Connect capture to playback (loopback) in the conference bridge
    AudioMedia &capMedia = m_endpoint->audDevManager().getCaptureDevMedia();
    AudioMedia &playMedia = m_endpoint->audDevManager().getPlaybackDevMedia();
    capMedia.startTransmit(playMedia);
    useLoudspeaker(true);
    qDebug() << "Started audio loopback test on capture=" << captureIdx
             << "playback=" << playbackIdx;
  } catch (Error &err) {
    qWarning() << "Error starting audio test:"
               << QString::fromStdString(err.info());
  }
}

void HardwareService::stopAudioTest() {
  if (!m_endpoint)
    return;
  try {
    AudioMedia &capMedia = m_endpoint->audDevManager().getCaptureDevMedia();
    AudioMedia &playMedia = m_endpoint->audDevManager().getPlaybackDevMedia();
    capMedia.stopTransmit(playMedia);
    useLoudspeaker(false);
    qDebug() << "Stopped audio loopback test";
  } catch (...) {
  }
}

void HardwareService::startVideoPreview(int devIdx) {
  if (!m_endpoint)
    return;
  try {
    stopVideoPreview();

    // devIdx is a position in the camera list shown to the user
    if (devIdx < 0 || devIdx >= m_videoDeviceIndices.size()) {
      qWarning() << "HardwareService: Invalid video device index" << devIdx;
      return;
    }
    devIdx = m_videoDeviceIndices[devIdx];
    // "System default" is previewed as the first real camera: the preview
    // needs a concrete device to take a picture format from
    for (int candidate : m_videoDeviceIndices) {
      if (devIdx < 0 && candidate >= 0)
        devIdx = candidate;
    }
    if (devIdx < 0) {
      qWarning() << "HardwareService: no camera to preview";
      return;
    }

    VideoPreviewOpParam prm;
    prm.rendId = PJMEDIA_VID_DEFAULT_RENDER_DEV;
    prm.window.type = PJMEDIA_VID_DEV_HWND_TYPE_NONE;
    prm.show = false; // Settings draws the pictures itself (VideoSink)

    // PJSIP aborts on a preview without a picture format, so one of the
    // camera's own formats is always given, a common one if it has it
    const VideoDevInfo devInfo = m_endpoint->vidDevManager().getDevInfo(devIdx);
    if (devInfo.fmt.empty()) {
      qWarning() << "HardwareService: camera" << devIdx << "reports no formats";
      return;
    }
    const MediaFormatVideo *chosen = &devInfo.fmt[0];
    for (pj_uint32_t id : {PJMEDIA_FORMAT_RGB24, PJMEDIA_FORMAT_YUY2,
                           PJMEDIA_FORMAT_I420}) {
      for (const auto &fmt : devInfo.fmt) {
        if (fmt.id == id)
          chosen = &fmt; // the last one listed above that matches wins
      }
    }
    prm.format.id = chosen->id;
    prm.format.type = PJMEDIA_TYPE_VIDEO;
    prm.format.width = chosen->width;
    prm.format.height = chosen->height;
    prm.format.fpsNum = chosen->fpsNum;
    prm.format.fpsDenum = chosen->fpsDenum;

    m_videoPreview = new VideoPreview(devIdx);
    m_previewDevice = devIdx;
    m_videoPreview->start(prm);
    emit videoPreviewChanged();
    qDebug() << "HardwareService: Video preview started for device" << devIdx;
  } catch (Error &err) {
    qWarning() << "Error starting video preview:"
               << QString::fromStdString(err.info());
    if (m_videoPreview) {
      delete m_videoPreview;
      m_videoPreview = nullptr;
    }
    emit videoPreviewChanged();
  }
}

int HardwareService::previewConfPort() const {
#if PJSUA_HAS_VIDEO
  if (m_videoPreview)
    return pjsua_vid_preview_get_vid_conf_port(m_previewDevice);
#endif
  return -1;
}

void HardwareService::stopVideoPreview() {
  if (m_videoPreview) {
    try {
      m_videoPreview->stop();
    } catch (...) {
    }
    delete m_videoPreview;
    m_videoPreview = nullptr;
    emit videoPreviewChanged();
  }
}

float HardwareService::speakerVolume() const {
  if (!m_endpoint || m_speakerVolumeUnsupported)
    return 1.0f;
  try {
    unsigned vol = m_endpoint->audDevManager().getOutputVolume();
    return vol / 100.0f;
  } catch (...) {
    // Many devices have no level control. PJSIP logs an error each time it
    // is asked, so ask once.
    m_speakerVolumeUnsupported = true;
    return 1.0f;
  }
}

void HardwareService::setSpeakerVolume(float volume) {
  if (!m_endpoint)
    return;
  try {
    unsigned vol = static_cast<unsigned>(volume * 100);
    m_endpoint->audDevManager().setOutputVolume(vol);
    emit volumeChanged();
  } catch (...) {
  }
}

float HardwareService::micVolume() const {
  if (!m_endpoint || m_micVolumeUnsupported)
    return 1.0f;
  try {
    unsigned vol = m_endpoint->audDevManager().getInputVolume();
    return vol / 100.0f;
  } catch (...) {
    // Many devices have no level control. PJSIP logs an error each time it
    // is asked, so ask once.
    m_micVolumeUnsupported = true;
    return 1.0f;
  }
}

void HardwareService::setMicVolume(float volume) {
  if (!m_endpoint)
    return;
  try {
    unsigned vol = static_cast<unsigned>(volume * 100);
    m_endpoint->audDevManager().setInputVolume(vol);
    emit volumeChanged();
  } catch (...) {
  }
}

bool HardwareService::echoCancellation() const { return m_echoCancellation; }

void HardwareService::setEchoCancellation(bool enabled) {
  if (!m_endpoint)
    return;
  try {
    m_endpoint->audDevManager().setEcOptions(enabled ? 200 : 0, 0);
    m_echoCancellation = enabled;
    emit echoCancellationChanged();
  } catch (...) {
  }
}
