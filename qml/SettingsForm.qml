import QtQuick
import QtQuick.Layouts
import "Scale.js" as S

Item {
    id: root

    // The main window's key handler calls these on the current stack page.
    function move(delta) {}
    function activate() {}
    function enqueue() {}

    implicitHeight: flick.contentHeight

    // 0 = the app, 1 = the tray panel, 2 = the library folders.
    property int section: 0

    Segmented {
        id: sectionTabs
        x: S.sp(16)
        y: S.sp(12)
        options: [qsTr("App"), qsTr("Tray panel"), qsTr("Library")]
        selected: options[root.section]
        onPicked: (v) => { root.section = options.indexOf(v); flick.contentY = 0 }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.topMargin: sectionTabs.y + sectionTabs.height + S.sp(4)
        contentWidth: width
        contentHeight: column.implicitHeight + S.sp(24)
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        FastWheel { view: flick }

        ColumnLayout {
            id: column
            x: S.sp(16)
            y: S.sp(12)
            width: Math.min(flick.width - S.sp(32), S.sp(460))
            spacing: S.sp(14)

            LookForm { Layout.fillWidth: true; visible: root.section === 0; target: ui }
            UiText { visible: root.section === 0; text: qsTr("Startup"); color: theme.dimForeground; px: 12 }
            ToggleRow {
                visible: root.section === 0
                Layout.fillWidth: true
                label: qsTr("Start at login (in the tray)")
                checked: autostart.enabled
                onToggled: (v) => autostart.enabled = v
            }
            UiText { visible: root.section === 0; text: qsTr("Left panel: the tabs to show"); color: theme.dimForeground; px: 12 }
            Repeater {
                model: ["Albums", "Artists", "Songs", "Playlists", "Libraries", "Queue"]
                ToggleRow {
                    required property string modelData
                    visible: root.section === 0
                    Layout.fillWidth: true
                    label: modelData
                    checked: settingsStore.hiddenTabs.indexOf(modelData) < 0
                    onToggled: (v) => settingsStore.setTabShown(modelData, v)
                }
            }
            UiText { visible: root.section === 0; text: qsTr("Lists"); color: theme.dimForeground; px: 12 }
            ToggleRow {
                visible: root.section === 0
                Layout.fillWidth: true
                label: qsTr("Show local / server tags on songs")
                checked: settingsStore.showSourceTags
                onToggled: (v) => settingsStore.set("showSourceTags", v)
            }
            ToggleRow {
                visible: root.section === 0
                Layout.fillWidth: true
                label: qsTr("Remember sort order")
                checked: settingsStore.saveSort
                onToggled: (v) => settingsStore.set("saveSort", v)
            }
            UiText { visible: root.section === 0; text: qsTr("Layout"); color: theme.dimForeground; px: 12 }
            ToggleRow {
                visible: root.section === 0
                Layout.fillWidth: true
                label: qsTr("Remember column sizes")
                checked: settingsStore.saveLayout
                onToggled: (v) => settingsStore.set("saveLayout", v)
            }
            Rectangle {
                visible: root.section === 0
                Layout.preferredWidth: resetText.implicitWidth + S.sp(24)
                Layout.preferredHeight: S.sp(30)
                radius: S.sp(6)
                color: resetArea.containsMouse ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
                border.width: 1
                border.color: resetArea.containsMouse ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12)
                UiText { id: resetText; anchors.centerIn: parent; text: qsTr("Reset column sizes") }
                MouseArea {
                    id: resetArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settingsStore.resetLayout()
                }
            }
            LookForm { Layout.fillWidth: true; visible: root.section === 1; target: panelLook }
            LibraryForm { Layout.fillWidth: true; visible: root.section === 2 }
            ServerForm { Layout.fillWidth: true; visible: root.section === 2 }

            UiText { visible: root.section === 1; text: qsTr("Panel sections"); color: theme.dimForeground; px: 12 }
            ToggleRow { visible: root.section === 1; Layout.fillWidth: true; label: qsTr("Cover"); checked: panelLook.panelCover; onToggled: (v) => panelLook.set("panelCover", v) }
            ToggleRow { visible: root.section === 1; Layout.fillWidth: true; label: qsTr("Seek bar"); checked: panelLook.panelSeek; onToggled: (v) => panelLook.set("panelSeek", v) }
            ToggleRow { visible: root.section === 1; Layout.fillWidth: true; label: qsTr("Volume"); checked: panelLook.panelVolume; onToggled: (v) => panelLook.set("panelVolume", v) }
            ToggleRow { visible: root.section === 1; Layout.fillWidth: true; label: qsTr("Up next list"); checked: panelLook.panelUpNext; onToggled: (v) => panelLook.set("panelUpNext", v) }

            UiText { visible: root.section === 1; text: qsTr("Up next rows"); color: theme.dimForeground; px: 12 }
            Stepper {
                visible: root.section === 1
                value: panelLook.panelUpNextCount
                from: 0
                to: 10
                onPicked: (v) => panelLook.set("panelUpNextCount", v)
            }
        }
    }

    // Scroll hint: shown while there is more content below.
    Rectangle {
        id: chevron
        visible: flick.contentHeight - flick.contentY - flick.height > S.sp(4)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: S.sp(6)
        width: S.sp(36)
        height: S.sp(22)
        radius: height / 2
        color: theme.selection
        opacity: chevronArea.containsMouse ? 1.0 : 0.85
        UiText { anchors.centerIn: parent; text: "▾"; color: theme.accent }
        MouseArea {
            id: chevronArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: flick.contentY = Math.min(flick.contentHeight - flick.height,
                                                 flick.contentY + flick.height * 0.8)
        }
    }
}
