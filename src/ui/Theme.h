#ifndef THEME_H
#define THEME_H

#include <QObject>
#include <QString>

class Theme : public QObject {
  Q_OBJECT

  // Theme Mode
  Q_PROPERTY(bool isDarkMode READ isDarkMode WRITE setIsDarkMode NOTIFY
                 isDarkModeChanged)

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
  Q_PROPERTY(int fontSizeCaption READ fontSizeCaption CONSTANT)
  Q_PROPERTY(int fontSizeBody READ fontSizeBody CONSTANT)
  Q_PROPERTY(int fontSizeSubtitle READ fontSizeSubtitle CONSTANT)
  Q_PROPERTY(int fontSizeTitle READ fontSizeTitle CONSTANT)
  Q_PROPERTY(int fontSizeDisplay READ fontSizeDisplay CONSTANT)

  // Spacing
  Q_PROPERTY(int spacing1 READ spacing1 CONSTANT)
  Q_PROPERTY(int spacing2 READ spacing2 CONSTANT)
  Q_PROPERTY(int spacing3 READ spacing3 CONSTANT)
  Q_PROPERTY(int spacing4 READ spacing4 CONSTANT)
  Q_PROPERTY(int spacing6 READ spacing6 CONSTANT)
  Q_PROPERTY(int spacing8 READ spacing8 CONSTANT)
  Q_PROPERTY(int spacing12 READ spacing12 CONSTANT)
  Q_PROPERTY(int spacing16 READ spacing16 CONSTANT)

  // Border Radius
  Q_PROPERTY(int radiusSmall READ radiusSmall CONSTANT)
  Q_PROPERTY(int radiusMedium READ radiusMedium CONSTANT)
  Q_PROPERTY(int radiusLarge READ radiusLarge CONSTANT)
  Q_PROPERTY(int radiusXLarge READ radiusXLarge CONSTANT)
  Q_PROPERTY(int radiusRound READ radiusRound CONSTANT)

  // Animation
  Q_PROPERTY(int durationFast READ durationFast CONSTANT)
  Q_PROPERTY(int durationNormal READ durationNormal CONSTANT)
  Q_PROPERTY(int durationSlow READ durationSlow CONSTANT)
  Q_PROPERTY(int easingStandard READ easingStandard CONSTANT)

  // Touch Targets
  Q_PROPERTY(int minTouchTarget READ minTouchTarget CONSTANT)
  Q_PROPERTY(int touchTargetComfortable READ touchTargetComfortable CONSTANT)

public:
  static Theme &instance() {
    static Theme theme;
    return theme;
  }

  bool isDarkMode() const { return m_isDarkMode; }
  void setIsDarkMode(bool darkMode) {
    if (m_isDarkMode != darkMode) {
      m_isDarkMode = darkMode;
      emit isDarkModeChanged();
    }
  }

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

  // Dark Mode
  QString darkBg1() const { return "#0F172A"; }
  QString darkBg2() const { return "#1E293B"; }
  QString darkBg3() const { return "#334155"; }
  QString darkText() const { return "#F1F5F9"; }
  QString darkTextSecondary() const { return "#94A3B8"; }

  // Light Mode
  QString lightBg1() const { return "#FFFFFF"; }
  QString lightBg2() const { return "#F3F4F6"; }
  QString lightBg3() const { return "#E5E7EB"; }
  QString lightText() const { return "#1F2937"; }
  QString lightTextSecondary() const { return "#6B7280"; }

  // Dynamic
  QString background() const { return m_isDarkMode ? darkBg1() : lightBg1(); }
  QString backgroundSecondary() const {
    return m_isDarkMode ? darkBg2() : lightBg2();
  }
  QString backgroundTertiary() const {
    return m_isDarkMode ? darkBg3() : lightBg3();
  }
  QString textPrimary() const {
    return m_isDarkMode ? darkText() : lightText();
  }
  QString textSecondary() const {
    return m_isDarkMode ? darkTextSecondary() : lightTextSecondary();
  }
  QString border() const { return m_isDarkMode ? gray700() : gray300(); }
  QString borderLight() const { return m_isDarkMode ? gray800() : gray200(); }

  // Typography
  QString fontFamily() const { return "Inter, Roboto, sans-serif"; }
  int fontSizeCaption() const { return 12; }
  int fontSizeBody() const { return 14; }
  int fontSizeSubtitle() const { return 16; }
  int fontSizeTitle() const { return 20; }
  int fontSizeDisplay() const { return 28; }

  // Spacing
  int spacing1() const { return 4; }
  int spacing2() const { return 8; }
  int spacing3() const { return 12; }
  int spacing4() const { return 16; }
  int spacing6() const { return 24; }
  int spacing8() const { return 32; }
  int spacing12() const { return 48; }
  int spacing16() const { return 64; }

  // Radius
  int radiusSmall() const { return 4; }
  int radiusMedium() const { return 8; }
  int radiusLarge() const { return 12; }
  int radiusXLarge() const { return 16; }
  int radiusRound() const { return 999; }

  // Animation
  int durationFast() const { return 100; }
  int durationNormal() const { return 200; }
  int durationSlow() const { return 300; }
  int easingStandard() const { return 3; } // Easing.OutCubic

  // Touch
  int minTouchTarget() const { return 44; }
  int touchTargetComfortable() const { return 48; }

signals:
  void isDarkModeChanged();

private:
  Theme(QObject *parent = nullptr) : QObject(parent), m_isDarkMode(true) {}
  Theme(const Theme &) = delete;
  Theme &operator=(const Theme &) = delete;

  bool m_isDarkMode;
};

#endif // THEME_H
