#include "Theme.h"

#include <QGuiApplication>
#include <QSettings>
#include <QStyleHints>

//                     name               dark   background surface   foreground muted     accent
static const Theme::Palette kPalettes[] = {
    {"Light",           false, "#F7F8FA", "#FFFFFF", "#1F2328", "#6B7280", "#2563EB"},
    {"Dark",            true,  "#1E1F22", "#2B2D31", "#E6E7E9", "#9AA0A6", "#5B9BFF"},
    {"Midnight",        true,  "#000000", "#141414", "#EDEDED", "#8A8F98", "#22D3EE"},
    {"Nord",            true,  "#2E3440", "#3B4252", "#ECEFF4", "#A7B1C2", "#88C0D0"},
    {"Solarized Light", false, "#FDF6E3", "#EEE8D5", "#586E75", "#93A1A1", "#268BD2"},
    {"Solarized Dark",  true,  "#002B36", "#073642", "#EEE8D5", "#839496", "#2AA198"},
};

static QSettings settingsFile() {
  // Named explicitly: also read before the application object sets its names
  return QSettings("Antigravity", "MicrosipClone");
}

Theme &Theme::instance() {
  static Theme theme;
  return theme;
}

Theme::Theme(QObject *parent) : QObject(parent) {
  loadSettings();
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  // "System" tracks the desktop switching between light and dark
  connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
          &Theme::changed);
#endif
}

QStringList Theme::names() const {
  QStringList names{"System"};
  for (const Palette &palette : kPalettes)
    names.append(palette.name);
  return names;
}

const Theme::Palette &Theme::palette() const {
  for (const Palette &palette : kPalettes) {
    if (m_current == QLatin1String(palette.name))
      return palette;
  }
  // "System", or a name from a newer version: light or dark as the desktop is
  bool dark = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  dark = QGuiApplication::styleHints()->colorScheme() != Qt::ColorScheme::Light;
#endif
  return kPalettes[dark ? 1 : 0];
}

void Theme::setCurrent(const QString &name) {
  if (m_current == name)
    return;
  m_current = name;
  emit changed();
}

void Theme::setUiScale(double scale) {
  if (qFuzzyCompare(m_uiScale, scale))
    return;
  m_uiScale = scale;
  emit changed();
}

void Theme::setCloseToTray(bool enabled) {
  if (m_closeToTray == enabled)
    return;
  m_closeToTray = enabled;
  emit changed();
}

double Theme::savedUiScale() {
  return settingsFile().value("ui/scale", 1.0).toDouble();
}

void Theme::loadSettings() {
  QSettings settings = settingsFile();
  // Before there were themes there was only a dark-mode switch
  const QString legacy =
      settings.contains("ui/darkMode")
          ? (settings.value("ui/darkMode").toBool() ? "Dark" : "Light")
          : "System";
  m_current = settings.value("ui/theme", legacy).toString();
  m_uiScale = settings.value("ui/scale", 1.0).toDouble();
  m_closeToTray = settings.value("ui/closeToTray", false).toBool();
  emit changed();
}

void Theme::saveSettings() {
  QSettings settings = settingsFile();
  settings.setValue("ui/theme", m_current);
  settings.setValue("ui/scale", m_uiScale);
  settings.setValue("ui/closeToTray", m_closeToTray);
  settings.remove("ui/darkMode");
}
