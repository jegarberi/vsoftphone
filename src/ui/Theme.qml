pragma Singleton
import QtQuick

QtObject {
    id: theme
    
    // Color Palette
    readonly property color primaryStart: "#4A90E2"
    readonly property color primaryEnd: "#357ABD"
    readonly property color accentGreen: "#10B981"
    readonly property color dangerRed: "#EF4444"
    readonly property color warningOrange: "#F59E0B"
    
    // Dark Mode Colors
    readonly property color darkBg1: "#0F172A"
    readonly property color darkBg2: "#1E293B"
    readonly property color darkBg3: "#334155"
    readonly property color darkText: "#F1F5F9"
    readonly property color darkTextSecondary: "#94A3B8"
    
    // Light Mode Colors
    readonly property color lightBg1: "#FFFFFF"
    readonly property color lightBg2: "#F3F4F6"
    readonly property color lightBg3: "#E5E7EB"
    readonly property color lightText: "#1F2937"
    readonly property color lightTextSecondary: "#6B7280"
    
    // Neutral Grays
    readonly property color gray900: "#111827"
    readonly property color gray800: "#1F2937"
    readonly property color gray700: "#374151"
    readonly property color gray600: "#4B5563"
    readonly property color gray500: "#6B7280"
    readonly property color gray400: "#9CA3AF"
    readonly property color gray300: "#D1D5DB"
    readonly property color gray200: "#E5E7EB"
    readonly property color gray100: "#F3F4F6"
    readonly property color gray50: "#F9FAFB"
    
    // Theme Mode
    property bool isDarkMode: true
    
    // Dynamic Colors based on theme
    readonly property color background: isDarkMode ? darkBg1 : lightBg1
    readonly property color backgroundSecondary: isDarkMode ? darkBg2 : lightBg2
    readonly property color backgroundTertiary: isDarkMode ? darkBg3 : lightBg3
    readonly property color textPrimary: isDarkMode ? darkText : lightText
    readonly property color textSecondary: isDarkMode ? darkTextSecondary : lightTextSecondary
    readonly property color border: isDarkMode ? gray700 : gray300
    readonly property color borderLight: isDarkMode ? gray800 : gray200
    
    // Typography
    readonly property int fontSizeCaption: 12
    readonly property int fontSizeBody: 14
    readonly property int fontSizeSubtitle: 16
    readonly property int fontSizeTitle: 20
    readonly property int fontSizeDisplay: 28
    readonly property string fontFamily: "Inter, Roboto, sans-serif"
    
    // Spacing (4px base unit)
    readonly property int spacing1: 4
    readonly property int spacing2: 8
    readonly property int spacing3: 12
    readonly property int spacing4: 16
    readonly property int spacing6: 24
    readonly property int spacing8: 32
    readonly property int spacing12: 48
    readonly property int spacing16: 64
    
    // Border Radius
    readonly property int radiusSmall: 4
    readonly property int radiusMedium: 8
    readonly property int radiusLarge: 12
    readonly property int radiusXLarge: 16
    readonly property int radiusRound: 999
    
    // Shadows
    readonly property string shadowSmall: "0 1px 2px rgba(0, 0, 0, 0.05)"
    readonly property string shadowMedium: "0 2px 8px rgba(0, 0, 0, 0.1)"
    readonly property string shadowLarge: "0 4px 16px rgba(0, 0, 0, 0.15)"
    
    // Animation Durations
    readonly property int durationFast: 100
    readonly property int durationNormal: 200
    readonly property int durationSlow: 300
    
    // Easing Curves
    readonly property var easingStandard: Easing.OutCubic
    readonly property var easingDecelerate: Easing.OutQuad
    readonly property var easingAccelerate: Easing.InQuad
    
    // Z-Index Layers
    readonly property int zIndexBase: 0
    readonly property int zIndexDropdown: 1000
    readonly property int zIndexModal: 2000
    readonly property int zIndexToast: 3000
    
    // Touch Targets
    readonly property int minTouchTarget: 44
    readonly property int touchTargetComfortable: 48
    
    // Gradients
    function primaryGradient() {
        return Qt.createQmlObject('import QtQuick 2.15; Gradient { GradientStop { position: 0.0; color: "' + primaryStart + '" } GradientStop { position: 1.0; color: "' + primaryEnd + '" } }', theme)
    }
}
