#include "HardwareService.h"
#include <QDebug>
#include <QSettings>
#include <pjsua2/endpoint.hpp>
#include <pjsua2/media.hpp>

using namespace pj;

HardwareService &HardwareService::instance() {
  static HardwareService _instance;
  return _instance;
}

HardwareService::HardwareService(QObject *parent)
    : QObject(parent), m_endpoint(nullptr), m_videoPreview(nullptr) {}

void HardwareService::init(pj::Endpoint *endpoint) {
  m_endpoint = endpoint;
  refreshDevices();
  restoreSettings();
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
  if (settings.contains("audio/speakerVolume"))
    setSpeakerVolume(settings.value("audio/speakerVolume").toFloat());
  if (settings.contains("audio/micVolume"))
    setMicVolume(settings.value("audio/micVolume").toFloat());
  if (settings.contains("audio/echoCancellation"))
    setEchoCancellation(settings.value("audio/echoCancellation").toBool());
}

void HardwareService::saveSettings() {
  QSettings settings;
  settings.setValue("audio/capture",
                    m_audioCaptureDevices.value(m_captureIndex));
  settings.setValue("audio/playback",
                    m_audioPlaybackDevices.value(m_playbackIndex));
  settings.setValue("audio/speakerVolume", speakerVolume());
  settings.setValue("audio/micVolume", micVolume());
  settings.setValue("audio/echoCancellation", m_echoCancellation);
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

    // Video Devices
    VidDevManager &vdm = m_endpoint->vidDevManager();
    VideoDevInfoVector2 videoDevs = vdm.enumDev2();
    qDebug() << "HardwareService: Found" << videoDevs.size() << "video devices";
    for (unsigned i = 0; i < videoDevs.size(); ++i) {
      const VideoDevInfo &info = videoDevs[i];
      QString name = QString::fromStdString(info.name);
      QString driver = QString::fromStdString(info.driver);
      QString friendlyName =
          driver.isEmpty() ? name : QString("%1 (%2)").arg(name, driver);
      if (info.dir == PJMEDIA_DIR_RENDER)
        continue; // a renderer is not a camera
      m_videoDevices.append(friendlyName);
      m_videoDeviceIndices.append(i);
      qDebug() << " - Video Device:" << friendlyName;
    }

    // Audio Codecs
    const CodecInfoVector2 &aCodecs = m_endpoint->codecEnum2();
    for (unsigned i = 0; i < aCodecs.size(); ++i) {
      m_audioCodecs.append(QString::fromStdString(aCodecs[i].codecId));
    }

    // Video Codecs
    const CodecInfoVector2 &vCodecs = m_endpoint->videoCodecEnum2();
    qDebug() << "HardwareService: Found" << vCodecs.size() << "video codecs";
    for (unsigned i = 0; i < vCodecs.size(); ++i) {
      m_videoCodecs.append(QString::fromStdString(vCodecs[i].codecId));
      qDebug() << " - Video Codec:"
               << QString::fromStdString(vCodecs[i].codecId);
    }

  } catch (Error &err) {
    qWarning() << "Error refreshing devices:"
               << QString::fromStdString(err.info());
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
    if (devIdx >= m_videoDeviceIndices.size())
      devIdx = -1;
    if (devIdx >= 0)
      devIdx = m_videoDeviceIndices[devIdx];
    if (devIdx < 0) {
      qWarning() << "HardwareService: Invalid video device index" << devIdx;
      return;
    }

    m_videoPreview = new VideoPreview(devIdx);
    VideoPreviewOpParam prm;
    prm.rendId = PJMEDIA_VID_DEFAULT_RENDER_DEV;

    // Explicitly set window type for Linux
    prm.window.type = PJMEDIA_VID_DEV_HWND_TYPE_NONE;

    // Try to find a native format to avoid assertion failure in vid_port.c
    try {
      VidDevManager &mgr = m_endpoint->vidDevManager();
      VideoDevInfo devInfo = mgr.getDevInfo(devIdx);
      if (!devInfo.fmt.empty()) {
        const MediaFormatVideo *chosen = nullptr;
        // Prefer some common formats
        pj_uint32_t preferred[] = {PJMEDIA_FORMAT_I420, PJMEDIA_FORMAT_YUY2,
                                   PJMEDIA_FORMAT_RGB24};
        for (auto id : preferred) {
          for (const auto &fmt : devInfo.fmt) {
            if (fmt.id == id) {
              chosen = &fmt;
              break;
            }
          }
          if (chosen)
            break;
        }
        if (!chosen)
          chosen = &devInfo.fmt[0];

        prm.format.id = chosen->id;
        prm.format.type = PJMEDIA_TYPE_VIDEO;
        prm.format.width = chosen->width;
        prm.format.height = chosen->height;
        prm.format.fpsNum = chosen->fpsNum;
        prm.format.fpsDenum = chosen->fpsDenum;
      }
    } catch (...) {
      // Fallback or ignore
    }

    m_videoPreview->start(prm);
    qDebug() << "HardwareService: Video preview started for device" << devIdx;
  } catch (Error &err) {
    qWarning() << "Error starting video preview:"
               << QString::fromStdString(err.info());
    if (m_videoPreview) {
      delete m_videoPreview;
      m_videoPreview = nullptr;
    }
  }
}

void HardwareService::stopVideoPreview() {
  if (m_videoPreview) {
    try {
      m_videoPreview->stop();
    } catch (...) {
    }
    delete m_videoPreview;
    m_videoPreview = nullptr;
  }
}

float HardwareService::speakerVolume() const {
  if (!m_endpoint)
    return 1.0f;
  try {
    unsigned vol = m_endpoint->audDevManager().getOutputVolume();
    return vol / 100.0f;
  } catch (...) {
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
  if (!m_endpoint)
    return 1.0f;
  try {
    unsigned vol = m_endpoint->audDevManager().getInputVolume();
    return vol / 100.0f;
  } catch (...) {
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
