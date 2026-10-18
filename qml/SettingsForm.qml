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

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight + S.sp(24)
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: column
            x: S.sp(16)
            y: S.sp(12)
            width: flick.width - S.sp(32)
            spacing: S.sp(14)

            UiText { text: qsTr("App"); font.bold: true; color: theme.accent }
            LookForm { Layout.fillWidth: true; target: ui }

            UiText { text: qsTr("Tray panel"); font.bold: true; color: theme.accent; Layout.topMargin: S.sp(10) }
            LookForm { Layout.fillWidth: true; target: panelLook }

            UiText { text: qsTr("Panel sections"); color: theme.dimForeground; px: 12 }
            ToggleRow { Layout.fillWidth: true; label: qsTr("Cover"); checked: panelLook.panelCover; onToggled: (v) => panelLook.set("panelCover", v) }
            ToggleRow { Layout.fillWidth: true; label: qsTr("Seek bar"); checked: panelLook.panelSeek; onToggled: (v) => panelLook.set("panelSeek", v) }
            ToggleRow { Layout.fillWidth: true; label: qsTr("Volume"); checked: panelLook.panelVolume; onToggled: (v) => panelLook.set("panelVolume", v) }
            ToggleRow { Layout.fillWidth: true; label: qsTr("Up next list"); checked: panelLook.panelUpNext; onToggled: (v) => panelLook.set("panelUpNext", v) }

            UiText { text: qsTr("Up next rows"); color: theme.dimForeground; px: 12 }
            Stepper {
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
