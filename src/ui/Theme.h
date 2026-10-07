#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QObject>
#include <QString>
#include <QStringList>

// Appearance preferences, chiefly the colour themes. QML windows feed these
// colours to the Material style,
// which then themes every standard control; the few extra roles a softphone
// needs (muted text, success, danger) are here too.
//
// "System" follows the desktop's light or dark setting. A change is shown at
// once so Settings can preview it; saveSettings() keeps it and loadSettings()
// goes back to what was saved.
class Theme : public QObject {
  Q_OBJECT
  Q_PROPERTY(QStringList names READ names CONSTANT)
  Q_PROPERTY(QString current READ current WRITE setCurrent NOTIFY changed)
  Q_PROPERTY(bool dark READ dark NOTIFY changed)
  Q_PROPERTY(QColor background READ background NOTIFY changed)
  Q_PROPERTY(QColor surface READ surface NOTIFY changed)
  Q_PROPERTY(QColor foreground READ foreground NOTIFY changed)
  Q_PROPERTY(QColor muted READ muted NOTIFY changed)
  Q_PROPERTY(QColor accent READ accent NOTIFY changed)
  Q_PROPERTY(QColor success READ success CONSTANT)
  Q_PROPERTY(QColor danger READ danger CONSTANT)
  Q_PROPERTY(QColor warning READ warning CONSTANT)
  // Whole-interface zoom, applied by Qt at startup: a change needs a restart
  Q_PROPERTY(double uiScale READ uiScale WRITE setUiScale NOTIFY changed)
  // Closing the main window hides it in the system tray instead of quitting
  Q_PROPERTY(bool closeToTray READ closeToTray WRITE setCloseToTray NOTIFY
                 changed)

public:
  struct Palette {
    const char *name;
    bool dark;
    const char *background;
    const char *surface;
    const char *foreground;
    const char *muted;
    const char *accent;
  };

  static Theme &instance();

  QStringList names() const;
  QString current() const { return m_current; }
  void setCurrent(const QString &name);

  bool dark() const { return palette().dark; }
  QColor background() const { return QColor(palette().background); }
  QColor surface() const { return QColor(palette().surface); }
  QColor foreground() const { return QColor(palette().foreground); }
  QColor muted() const { return QColor(palette().muted); }
  QColor accent() const { return QColor(palette().accent); }
  QColor success() const { return QColor("#22C55E"); }
  QColor danger() const { return QColor("#EF4444"); }
  QColor warning() const { return QColor("#F59E0B"); }

  double uiScale() const { return m_uiScale; }
  void setUiScale(double scale);
  bool closeToTray() const { return m_closeToTray; }
  void setCloseToTray(bool enabled);

  Q_INVOKABLE void loadSettings();
  Q_INVOKABLE void saveSettings();

  // The saved zoom, readable before the application object exists
  static double savedUiScale();

signals:
  void changed();

private:
  explicit Theme(QObject *parent = nullptr);
  Theme(const Theme &) = delete;
  Theme &operator=(const Theme &) = delete;

  const Palette &palette() const;

  QString m_current = "System";
  double m_uiScale = 1.0;
  bool m_closeToTray = false;
};

#endif // THEME_H
