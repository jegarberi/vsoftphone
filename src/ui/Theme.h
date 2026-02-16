#ifndef THEME_H
#define THEME_H

#include <QObject>
#include <QString>

class Theme : public QObject {
  Q_OBJECT

  // Theme Mode
  Q_PROPERTY(bool isDarkMode READ isDarkMode WRITE setIsDarkMode NOTIFY
                 isDarkModeChanged)

  // UI Scaling
  Q_PROPERTY(double uiScale READ uiScale WRITE setUiScale NOTIFY uiScaleChanged)

  // Colors
  Q_PROPERTY(QString primaryStart READ primaryStart CONSTANT)
  Q_PROPERTY(QString primaryEnd READ primaryEnd CONSTANT)
  Q_PROPERTY(QString accentGreen READ accentGreen CONSTANT)
  Q_PROPERTY(QString dangerRed READ dangerRed CONSTANT)
  Q_PROPERTY(QString warningOrange READ warningOrange CONSTANT)

  // Neutral Grays
  Q_PROPERTY(QString gray900 READ gray900 CONSTANT)
  Q_PROPERTY(QString gray800 READ gray800 CONSTANT)
  Q_PROPERTY(QString gray700 READ gray700 CONSTANT)
  Q_PROPERTY(QString gray600 READ gray600 CONSTANT)
  Q_PROPERTY(QString gray500 READ gray500 CONSTANT)
  Q_PROPERTY(QString gray400 READ gray400 CONSTANT)
  Q_PROPERTY(QString gray300 READ gray300 CONSTANT)
  Q_PROPERTY(QString gray200 READ gray200 CONSTANT)
  Q_PROPERTY(QString gray100 READ gray100 CONSTANT)
  Q_PROPERTY(QString gray50 READ gray50 CONSTANT)

  // Dynamic Colors
  Q_PROPERTY(QString background READ background NOTIFY isDarkModeChanged)
  Q_PROPERTY(QString backgroundSecondary READ backgroundSecondary NOTIFY
                 isDarkModeChanged)
  Q_PROPERTY(QString backgroundTertiary READ backgroundTertiary NOTIFY
                 isDarkModeChanged)
  Q_PROPERTY(QString textPrimary READ textPrimary NOTIFY isDarkModeChanged)
  Q_PROPERTY(QString textSecondary READ textSecondary NOTIFY isDarkModeChanged)
  Q_PROPERTY(QString border READ border NOTIFY isDarkModeChanged)
  Q_PROPERTY(QString borderLight READ borderLight NOTIFY isDarkModeChanged)

  // Typography
  Q_PROPERTY(QString fontFamily READ fontFamily CONSTANT)
  Q_PROPERTY(int fontSizeCaption READ fontSizeCaption NOTIFY uiScaleChanged)
  Q_PROPERTY(int fontSizeBody READ fontSizeBody NOTIFY uiScaleChanged)
  Q_PROPERTY(int fontSizeSubtitle READ fontSizeSubtitle NOTIFY uiScaleChanged)
  Q_PROPERTY(int fontSizeTitle READ fontSizeTitle NOTIFY uiScaleChanged)
  Q_PROPERTY(int fontSizeDisplay READ fontSizeDisplay NOTIFY uiScaleChanged)

  // Spacing
  Q_PROPERTY(int spacing1 READ spacing1 NOTIFY uiScaleChanged)
  Q_PROPERTY(int spacing2 READ spacing2 NOTIFY uiScaleChanged)
  Q_PROPERTY(int spacing3 READ spacing3 NOTIFY uiScaleChanged)
  Q_PROPERTY(int spacing4 READ spacing4 NOTIFY uiScaleChanged)
  Q_PROPERTY(int spacing6 READ spacing6 NOTIFY uiScaleChanged)
  Q_PROPERTY(int spacing8 READ spacing8 NOTIFY uiScaleChanged)
  Q_PROPERTY(int spacing12 READ spacing12 NOTIFY uiScaleChanged)
  Q_PROPERTY(int spacing16 READ spacing16 NOTIFY uiScaleChanged)

  // Border Radius
  Q_PROPERTY(int radiusSmall READ radiusSmall NOTIFY uiScaleChanged)
  Q_PROPERTY(int radiusMedium READ radiusMedium NOTIFY uiScaleChanged)
  Q_PROPERTY(int radiusLarge READ radiusLarge NOTIFY uiScaleChanged)
  Q_PROPERTY(int radiusXLarge READ radiusXLarge NOTIFY uiScaleChanged)
  Q_PROPERTY(int radiusRound READ radiusRound CONSTANT)

  // Animation
  Q_PROPERTY(int durationFast READ durationFast CONSTANT)
  Q_PROPERTY(int durationNormal READ durationNormal CONSTANT)
  Q_PROPERTY(int durationSlow READ durationSlow CONSTANT)
  Q_PROPERTY(int easingStandard READ easingStandard CONSTANT)

  // Touch Targets
  Q_PROPERTY(int minTouchTarget READ minTouchTarget NOTIFY uiScaleChanged)
  Q_PROPERTY(int touchTargetComfortable READ touchTargetComfortable NOTIFY
                 uiScaleChanged)

public:
  static Theme &instance();

  bool isDarkMode() const { return m_isDarkMode; }
  void setIsDarkMode(bool darkMode);

  double uiScale() const { return m_uiScale; }
  void setUiScale(double scale);

  Q_INVOKABLE void loadSettings();
  Q_INVOKABLE void saveSettings();

  // Color Palette
  QString primaryStart() const { return "#4A90E2"; }
  QString primaryEnd() const { return "#357ABD"; }
  QString accentGreen() const { return "#10B981"; }
  QString dangerRed() const { return "#EF4444"; }
  QString warningOrange() const { return "#F59E0B"; }

  // Grays
  QString gray900() const { return "#111827"; }
  QString gray800() const { return "#1F2937"; }
  QString gray700() const { return "#374151"; }
  QString gray600() const { return "#4B5563"; }
  QString gray500() const { return "#6B7280"; }
  QString gray400() const { return "#9CA3AF"; }
  QString gray300() const { return "#D1D5DB"; }
  QString gray200() const { return "#E5E7EB"; }
  QString gray100() const { return "#F3F4F6"; }
  QString gray50() const { return "#F9FAFB"; }

  // Dynamic
  QString background() const;
  QString backgroundSecondary() const;
  QString backgroundTertiary() const;
  QString textPrimary() const;
  QString textSecondary() const;
  QString border() const;
  QString borderLight() const;

  // Typography
  QString fontFamily() const { return "Inter, Roboto, sans-serif"; }
  int fontSizeCaption() const { return 12 * m_uiScale; }
  int fontSizeBody() const { return 14 * m_uiScale; }
  int fontSizeSubtitle() const { return 16 * m_uiScale; }
  int fontSizeTitle() const { return 20 * m_uiScale; }
  int fontSizeDisplay() const { return 28 * m_uiScale; }

  // Spacing
  int spacing1() const { return 4 * m_uiScale; }
  int spacing2() const { return 8 * m_uiScale; }
  int spacing3() const { return 12 * m_uiScale; }
  int spacing4() const { return 16 * m_uiScale; }
  int spacing6() const { return 24 * m_uiScale; }
  int spacing8() const { return 32 * m_uiScale; }
  int spacing12() const { return 48 * m_uiScale; }
  int spacing16() const { return 64 * m_uiScale; }

  // Radius
  int radiusSmall() const { return 4 * m_uiScale; }
  int radiusMedium() const { return 8 * m_uiScale; }
  int radiusLarge() const { return 12 * m_uiScale; }
  int radiusXLarge() const { return 16 * m_uiScale; }
  int radiusRound() const { return 999; }

  // Animation
  int durationFast() const { return 100; }
  int durationNormal() const { return 200; }
  int durationSlow() const { return 300; }
  int easingStandard() const { return 3; } // Easing.OutCubic

  // Touch
  int minTouchTarget() const { return 44 * m_uiScale; }
  int touchTargetComfortable() const { return 48 * m_uiScale; }

signals:
  void isDarkModeChanged();
  void uiScaleChanged();

private:
  explicit Theme(QObject *parent = nullptr);
  Theme(const Theme &) = delete;
  Theme &operator=(const Theme &) = delete;

  bool m_isDarkMode;
  double m_uiScale;
};

#endif // THEME_H
