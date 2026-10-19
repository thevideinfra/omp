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

    // 0 = the app, 1 = the tray panel.
    property int section: 0

    Segmented {
        id: sectionTabs
        x: S.sp(16)
        y: S.sp(12)
        options: [qsTr("App"), qsTr("Tray panel")]
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
            LookForm { Layout.fillWidth: true; visible: root.section === 1; target: panelLook }

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
