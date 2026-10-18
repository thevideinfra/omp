import QtQuick
import QtQuick.Layouts
import "Fmt.js" as Fmt

Window {
    id: panel
    objectName: "panelWindow"
    title: "omamusic-panel"
    flags: Qt.Tool | Qt.FramelessWindowHint
    width: 360
    height: 540
    visible: false
    color: theme.background

    // Close on focus loss, but only after the window has been active once.
    property bool wasActive: false
    onActiveChanged: {
        if (active) {
            wasActive = true
        } else if (wasActive) {
            wasActive = false
            visible = false
        }
    }

    Item {
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: panel.visible = false
        Keys.onSpacePressed: player.togglePause()

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: theme.selection
            border.width: 1
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 200
                Layout.preferredHeight: 200
                color: theme.surface
                Image {
                    anchors.fill: parent
                    source: library.coverFor(player.current.path ?? "")
                    fillMode: Image.PreserveAspectCrop
                    sourceSize.width: 400
                    sourceSize.height: 400
                    asynchronous: true
                }
            }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: player.current.title ?? qsTr("Nothing playing")
                color: theme.foreground
                font.pixelSize: 16
                font.bold: true
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: player.current.artist ?? ""
                color: theme.dimForeground
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                Text { text: Fmt.time(player.position); color: theme.dimForeground; font.pixelSize: 12 }
                SeekBar {
                    Layout.fillWidth: true
                    value: player.duration > 0 ? player.position / player.duration : 0
                    onSeeked: (f) => player.seekTo(f * player.duration)
                }
                Text { text: Fmt.time(player.duration); color: theme.dimForeground; font.pixelSize: 12 }
            }

            Row {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8
                TransportButton { glyph: "◀◀"; onClicked: player.previous() }
                TransportButton { glyph: player.playing ? "❚❚" : "▶"; big: true; onClicked: player.togglePause() }
                TransportButton { glyph: "▶▶"; onClicked: player.next() }
            }

            RowLayout {
                Layout.fillWidth: true
                Text { text: qsTr("vol"); color: theme.dimForeground; font.pixelSize: 12 }
                SeekBar {
                    Layout.fillWidth: true
                    value: player.volume / 100
                    onSeeked: (f) => player.volume = Math.round(f * 100)
                }
            }

            Text { text: qsTr("Up next"); color: theme.dimForeground; font.pixelSize: 12 }
            Repeater {
                model: player.queue
                delegate: Text {
                    required property int index
                    required property string title
                    required property string subtitle
                    readonly property bool upcoming: index > player.currentIndex && index <= player.currentIndex + 4
                    visible: upcoming
                    Layout.fillWidth: true
                    Layout.preferredHeight: upcoming ? implicitHeight : 0
                    text: title + "  ·  " + subtitle
                    color: theme.foreground
                    elide: Text.ElideRight
                }
            }

            Item { Layout.fillHeight: true }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                radius: 6
                color: openArea.containsMouse ? theme.selection : theme.surface
                Text { anchors.centerIn: parent; text: qsTr("Open omamusic"); color: theme.foreground }
                MouseArea {
                    id: openArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: { shell.showMain(); shell.hidePanel() }
                }
            }
        }
    }
}
