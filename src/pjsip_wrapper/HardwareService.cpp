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

  // Restore saved audio settings; devices are matched by name
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

void HardwareService::refreshDevices() {
  if (!m_endpoint)
    return;

  m_audioPlaybackDevices.clear();
  m_audioCaptureDevices.clear();
  m_playbackDeviceIndices.clear();
  m_captureDeviceIndices.clear();
  m_videoDevices.clear();
  m_audioCodecs.clear();
  m_videoCodecs.clear();

  try {
    // Audio Devices
    AudDevManager &adm = m_endpoint->audDevManager();
    AudioDevInfoVector2 audioDevs = adm.enumDev2();
    qDebug() << "HardwareService: Found" << audioDevs.size() << "audio devices";

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
      m_videoDevices.append(friendlyName);
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
      QSettings().setValue("audio/capture", m_audioCaptureDevices[captureIdx]);
    }
    if (playbackIdx >= 0 && playbackIdx < m_playbackDeviceIndices.size()) {
      int pjPlaybackIdx = m_playbackDeviceIndices[playbackIdx];
      if (pjPlaybackIdx == -99) {
        m_endpoint->audDevManager().setNullDev();
      } else {
        m_endpoint->audDevManager().setPlaybackDev(pjPlaybackIdx);
      }
      m_playbackIndex = playbackIdx;
      QSettings().setValue("audio/playback",
                           m_audioPlaybackDevices[playbackIdx]);
    }
  } catch (Error &err) {
    qWarning() << "Error setting audio device:"
               << QString::fromStdString(err.info());
  }
}

void HardwareService::setVideoDevice(int devIdx) {
  // PJSIP sets default video device differently if needed,
  // but often it's done during call or preview.
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

    // Check if device index is valid
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
    QSettings().setValue("audio/speakerVolume", volume);
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
    QSettings().setValue("audio/micVolume", volume);
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
    QSettings().setValue("audio/echoCancellation", enabled);
    emit echoCancellationChanged();
  } catch (...) {
  }
}
