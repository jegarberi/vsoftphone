#include "Theme.h"
#include <QSettings>

Theme &Theme::instance() {
  static Theme theme;
  return theme;
}

Theme::Theme(QObject *parent)
    : QObject(parent), m_isDarkMode(true), m_uiScale(1.0) {
  loadSettings();
}

void Theme::setIsDarkMode(bool darkMode) {
  if (m_isDarkMode != darkMode) {
    m_isDarkMode = darkMode;
    emit isDarkModeChanged();
  }
}

void Theme::setUiScale(double scale) {
  if (m_uiScale != scale) {
    m_uiScale = scale;
    emit uiScaleChanged();
  }
}

void Theme::loadSettings() {
  QSettings settings("Antigravity", "MicrosipClone");
  m_isDarkMode = settings.value("ui/darkMode", true).toBool();
  m_uiScale = settings.value("ui/scale", 1.0).toDouble();
  emit isDarkModeChanged();
  emit uiScaleChanged();
}

void Theme::saveSettings() {
  QSettings settings("Antigravity", "MicrosipClone");
  settings.setValue("ui/darkMode", m_isDarkMode);
  settings.setValue("ui/scale", m_uiScale);
}

// Dark Mode helpers
QString Theme::background() const {
  return m_isDarkMode ? "#0F172A" : "#FFFFFF";
}

QString Theme::backgroundSecondary() const {
  return m_isDarkMode ? "#1E293B" : "#F3F4F6";
}

QString Theme::backgroundTertiary() const {
  return m_isDarkMode ? "#334155" : "#E5E7EB";
}

QString Theme::textPrimary() const {
  return m_isDarkMode ? "#F1F5F9" : "#1F2937";
}

QString Theme::textSecondary() const {
  return m_isDarkMode ? "#94A3B8" : "#6B7280";
}

QString Theme::border() const { return m_isDarkMode ? "#374151" : "#D1D5DB"; }

QString Theme::borderLight() const {
  return m_isDarkMode ? "#1F2937" : "#E5E7EB";
}
