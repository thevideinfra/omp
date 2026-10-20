import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "Scale.js" as S

// The folders OMP reads music from. Changes rescan straight away.
ColumnLayout {
    id: root
    spacing: S.sp(10)

    UiText { text: qsTr("Music folders"); color: theme.dimForeground; px: 12 }

    Repeater {
        model: settingsStore.folders
        Rectangle {
            id: row
            required property string modelData
            Layout.fillWidth: true
            Layout.preferredHeight: S.sp(36)
            radius: S.sp(6)
            color: Qt.alpha(theme.foreground, 0.05)

            UiText {
                anchors.left: parent.left
                anchors.leftMargin: S.sp(10)
                anchors.right: removeChip.left
                anchors.rightMargin: S.sp(10)
                anchors.verticalCenter: parent.verticalCenter
                readonly property bool offline: library.offlineRoots.indexOf(row.modelData) >= 0
                text: offline ? row.modelData + qsTr("  (offline: showing last known songs)") : row.modelData
                color: offline ? theme.dimForeground : theme.foreground
                elide: Text.ElideMiddle
            }

            Rectangle {
                id: removeChip
                readonly property bool allowed: settingsStore.folders.length > 1
                anchors.right: parent.right
                anchors.rightMargin: S.sp(6)
                anchors.verticalCenter: parent.verticalCenter
                width: removeText.implicitWidth + S.sp(20)
                height: S.sp(26)
                radius: S.sp(6)
                opacity: allowed ? 1 : 0.4
                color: removeArea.containsMouse && allowed ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
                border.width: 1
                border.color: removeArea.containsMouse && allowed ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12)
                UiText { id: removeText; anchors.centerIn: parent; text: qsTr("Remove"); px: 12 }
                MouseArea {
                    id: removeArea
                    anchors.fill: parent
                    enabled: removeChip.allowed
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settingsStore.removeFolder(row.modelData)
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: S.sp(8)

        TextField {
            id: pathField
            Layout.fillWidth: true
            placeholderText: qsTr("Path, e.g. /mnt/nas/music")
            color: theme.foreground
            placeholderTextColor: theme.muted
            selectByMouse: true
            font.pixelSize: S.fs(13)
            background: Rectangle { radius: S.sp(6); color: theme.surface }
            onAccepted: addChip.add()
        }

        Rectangle {
            id: addChip
            function add() {
                settingsStore.addFolder(pathField.text)
                pathField.text = ""
            }
            Layout.preferredWidth: addText.implicitWidth + S.sp(20)
            Layout.preferredHeight: pathField.height
            radius: S.sp(6)
            color: addArea.containsMouse ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
            border.width: 1
            border.color: addArea.containsMouse ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12)
            UiText { id: addText; anchors.centerIn: parent; text: qsTr("Add") }
            MouseArea { id: addArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: addChip.add() }
        }

        Rectangle {
            Layout.preferredWidth: browseText.implicitWidth + S.sp(20)
            Layout.preferredHeight: pathField.height
            radius: S.sp(6)
            color: browseArea.containsMouse ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
            border.width: 1
            border.color: browseArea.containsMouse ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12)
            UiText { id: browseText; anchors.centerIn: parent; text: qsTr("Browse…") }
            MouseArea { id: browseArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: dialog.open() }
        }
    }

    FolderDialog {
        id: dialog
        title: qsTr("Add a music folder")
        onAccepted: settingsStore.addFolder(selectedFolder.toString())
    }

    UiText {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: qsTr("~/Music is the default. A mounted network share works like any other folder. The last folder cannot be removed.")
        color: theme.dimForeground
        px: 12
    }
}
