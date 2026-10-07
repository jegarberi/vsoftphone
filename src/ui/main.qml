import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1024
    height: 768
    visible: true
    title: qsTr("Microsip")
    
    color: Theme.background
    
    property int currentView: 0 // 0=Phone, 1=Contacts, 2=History, 3=Messages, 4=Accounts
    property bool sidebarCollapsed: width < 768
    
    // Loaders for windows
    Loader {
        id: settingsWindowLoader
        active: false
        source: "SettingsWindow.qml"
        onLoaded: {
            item.show()
            item.closing.connect(function() { settingsWindowLoader.active = false })
        }
    }
    
    
    // Call handling connections
    Connections {
        target: callService
        function onIncomingCall(callId, remoteUri) {
            var component = Qt.createComponent("InCallWindow.qml")
            var win = component.createObject(root, {
                "callId": callId,
                "remoteUri": remoteUri,
                "callState": "INCOMING",
                "isIncoming": true
            })
            win.show()
        }
        function onOutgoingCall(callId, remoteUri) {
            var component = Qt.createComponent("InCallWindow.qml")
            var win = component.createObject(root, {
                "callId": callId,
                "remoteUri": remoteUri,
                "callState": "CALLING"
            })
            win.show()
        }
    }
    
    RowLayout {
        anchors.fill: parent
        spacing: 0
        
        // Sidebar Navigation
        Rectangle {
            id: sidebar
            Layout.fillHeight: true
            Layout.preferredWidth: sidebarCollapsed ? 72 : 240
            color: Theme.backgroundSecondary
            
            Behavior on Layout.preferredWidth {
                NumberAnimation { duration: Theme.durationNormal; easing.type: Theme.easingStandard }
            }
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacing4
                spacing: Theme.spacing4
                
                // App Logo/Title
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacing3
                        spacing: Theme.spacing3
                        
                        Rectangle {
                            width: 40
                            height: 40
                            radius: Theme.radiusMedium
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: Theme.primaryStart }
                                GradientStop { position: 1.0; color: Theme.primaryEnd }
                            }
                            
                            Text {
                                anchors.centerIn: parent
                                text: "M"
                                font.family: Theme.fontFamily
                                font.pixelSize: 24
                                font.weight: Font.Bold
                                color: "white"
                            }
                        }
                        
                        Text {
                            visible: !sidebarCollapsed
                            text: "Microsip"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeTitle
                            font.weight: Font.Bold
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                        }
                    }
                }
                
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.border
                }
                
                // Navigation Items
                NavItem {
                    icon: "📞"
                    label: "Phone"
                    active: currentView === 0
                    collapsed: sidebarCollapsed
                    onClicked: currentView = 0
                }
                
                NavItem {
                    icon: "👥"
                    label: "Contacts"
                    active: currentView === 1
                    collapsed: sidebarCollapsed
                    onClicked: currentView = 1
                }
                
                NavItem {
                    icon: "🕐"
                    label: "History"
                    active: currentView === 2
                    collapsed: sidebarCollapsed
                    onClicked: currentView = 2
                }
                
                NavItem {
                    icon: "💬"
                    label: "Messages"
                    active: currentView === 3
                    collapsed: sidebarCollapsed
                    onClicked: currentView = 3
                }
                
                NavItem {
                    icon: "👤"
                    label: "Accounts"
                    active: currentView === 4
                    collapsed: sidebarCollapsed
                    onClicked: currentView = 4
                }
                
                Item { Layout.fillHeight: true }
                
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.border
                }
                
                // Settings Button
                NavItem {
                    icon: "⚙️"
                    label: "Settings"
                    active: false
                    collapsed: sidebarCollapsed
                    onClicked: settingsWindowLoader.active = true
                }
            }
        }
        
        // Main Content Area
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.background
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                
                // Header Bar
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    color: Theme.backgroundSecondary
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacing4
                        spacing: Theme.spacing4
                        
                        Item { Layout.fillWidth: true }

                        // Quick Actions
                        AppButton {
                            text: "📞"
                            variant: AppButton.Variant.Ghost
                            onClicked: currentView = 0
                        }
                        
                        AppButton {
                            text: "💬"
                            variant: AppButton.Variant.Ghost
                            onClicked: currentView = 3
                        }
                        
                        // User Avatar with Status
                        AppAvatar {
                            name: accountModel.defaultAccountId
                            showStatus: true
                            status: presenceService.status.toLowerCase()
                            size: 40
                        }
                    }
                }
                
                // Content Stack
                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: currentView
                    
                    DialpadView {
                        id: dialpadTab
                    }
                    
                    ContactsView {
                        id: contactsTab
                        onDialRequested: (number) => {
                            dialpadTab.dial(number)
                            currentView = 0
                        }
                    }
                    
                    HistoryView {
                        id: historyTab
                        onDialRequested: (number) => {
                            dialpadTab.dial(number)
                            currentView = 0
                        }
                    }
                    
                    MessageView {
                        id: messagesTab
                    }

                    AccountsView {
                        id: accountsTab
                    }
                }
            }
        }
    }
    
    // Navigation Item Component
    component NavItem: Rectangle {
        id: navItem
        property string icon: ""
        property string label: ""
        property bool active: false
        property bool collapsed: false
        signal clicked()
        
        Layout.fillWidth: true
        Layout.preferredHeight: 48
        radius: Theme.radiusMedium
        color: active ? Theme.primaryStart : (navItemMouse.containsMouse ? Theme.backgroundTertiary : "transparent")
        
        Behavior on color {
            ColorAnimation { duration: Theme.durationFast }
        }
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacing3
            spacing: Theme.spacing3
            
            Text {
                text: navItem.icon
                font.pixelSize: 24
                Layout.alignment: Qt.AlignVCenter
            }
            
            Text {
                visible: !navItem.collapsed
                text: navItem.label
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: navItem.active ? Font.Medium : Font.Normal
                color: navItem.active ? "white" : Theme.textPrimary
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
            }
        }
        
        MouseArea {
            id: navItemMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: navItem.clicked()
        }
    }
}
