import QtQuick
import QtQuick.Layouts
import "Fmt.js" as Fmt
import "Scale.js" as S

Window {
    id: panel
    objectName: "panelWindow"
    title: "omamusic-panel"
    flags: Qt.Tool | Qt.FramelessWindowHint
    width: 360
    height: Math.max(120, Math.min(680, col.implicitHeight + S.sp(32)))
    visible: false
    color: theme.background

    property bool editing: false

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
    onVisibleChanged: if (!visible) editing = false

    Item {
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: { if (panel.editing) panel.editing = false; else panel.visible = false }
        Keys.onSpacePressed: player.togglePause()

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: theme.selection
            border.width: 1
        }

        ColumnLayout {
            id: col
            x: S.sp(16)
            y: S.sp(16)
            width: parent.width - S.sp(32)
            spacing: S.sp(10)

            // player sections
            ColumnLayout {
                visible: !panel.editing
                Layout.fillWidth: true
                spacing: S.sp(10)

                Rectangle {
                    visible: ui.panelCover
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: S.sp(200)
                    Layout.preferredHeight: S.sp(200)
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

                UiText {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: player.current.title ?? qsTr("Nothing playing")
                    px: 16
                    font.bold: true
                    elide: Text.ElideRight
                }
                UiText {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: player.current.artist ?? ""
                    color: theme.dimForeground
                    elide: Text.ElideRight
                }

                RowLayout {
                    visible: ui.panelSeek
                    Layout.fillWidth: true
                    UiText { text: Fmt.time(player.position); color: theme.dimForeground; px: 12 }
                    SeekBar {
                        Layout.fillWidth: true
                        value: player.duration > 0 ? player.position / player.duration : 0
                        onSeeked: (f) => player.seekTo(f * player.duration)
                    }
                    UiText { text: Fmt.time(player.duration); color: theme.dimForeground; px: 12 }
                }

                Row {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: S.sp(8)
                    TransportButton { glyph: "◀◀"; onClicked: player.previous() }
                    TransportButton { glyph: player.playing ? "❚❚" : "▶"; big: true; onClicked: player.togglePause() }
                    TransportButton { glyph: "▶▶"; onClicked: player.next() }
                }

                RowLayout {
                    visible: ui.panelVolume
                    Layout.fillWidth: true
                    UiText { text: qsTr("vol"); color: theme.dimForeground; px: 12 }
                    SeekBar {
                        Layout.fillWidth: true
                        value: player.volume / 100
                        onSeeked: (f) => player.volume = Math.round(f * 100)
                    }
                }

                UiText {
                    visible: ui.panelUpNext && ui.panelUpNextCount > 0
                    text: qsTr("Up next")
                    color: theme.dimForeground
                    px: 12
                }
                Repeater {
                    model: ui.panelUpNext ? player.queue : null
                    delegate: UiText {
                        required property int index
                        required property string title
                        required property string subtitle
                        readonly property bool upcoming: index > player.currentIndex
                                                         && index <= player.currentIndex + ui.panelUpNextCount
                        visible: upcoming
                        Layout.fillWidth: true
                        Layout.preferredHeight: upcoming ? implicitHeight : 0
                        text: title + "  ·  " + subtitle
                        elide: Text.ElideRight
                    }
                }
            }

            // settings view
            SettingsForm {
                visible: panel.editing
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(S.sp(440), implicitHeight)
            }

            // footer
            RowLayout {
                Layout.fillWidth: true
                spacing: S.sp(8)

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: S.sp(34)
                    radius: S.sp(6)
                    color: mainArea.containsMouse ? theme.selection : theme.surface
                    UiText { anchors.centerIn: parent; text: panel.editing ? qsTr("Done") : qsTr("Open omamusic") }
                    MouseArea {
                        id: mainArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (panel.editing) {
                                panel.editing = false
                            } else {
                                shell.showMain()
                                shell.hidePanel()
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: S.sp(34)
                    Layout.preferredHeight: S.sp(34)
                    radius: S.sp(6)
                    color: panel.editing ? theme.selection : (gearArea.containsMouse ? theme.selection : theme.surface)
                    border.color: panel.editing ? theme.accent : "transparent"
                    border.width: 1
                    UiText { anchors.centerIn: parent; text: "⚙"; px: 16 }
                    MouseArea {
                        id: gearArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: panel.editing = !panel.editing
                    }
                }
            }
        }
    }
}
