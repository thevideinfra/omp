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

    // Hyprland ignores a floating window's own resizes after it is mapped, so
    // the height is fixed by freeze() just before the panel is shown and stays
    // put while it is open; anything that does not fit is clipped above the
    // pinned footer.
    readonly property int margin: S.sp(16)
    readonly property int gap: S.sp(10)
    readonly property int footerHeight: S.sp(34)
    readonly property int headerHeight: S.sp(30)
    readonly property int maxHeight: Math.min(680, (panel.screen ? panel.screen.height : 1080) - 80)
    readonly property int wantedHeight: Math.max(120, Math.min(maxHeight,
        headerHeight + gap + col.implicitHeight + gap + footerHeight + 2 * margin))
    property int frozenHeight: 0
    height: frozenHeight > 0 ? frozenHeight : wantedHeight
    function freeze() { frozenHeight = wantedHeight }

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
    onVisibleChanged: if (!visible) frozenHeight = 0

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

        // header: the settings gear, top right
        Item {
            id: header
            x: panel.margin
            y: panel.margin
            width: parent.width - 2 * panel.margin
            height: panel.headerHeight

            Rectangle {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: panel.headerHeight
                height: panel.headerHeight
                radius: S.sp(6)
                color: gearArea.containsMouse ? theme.selection : theme.surface
                UiText { anchors.centerIn: parent; text: "⚙"; px: 16 }
                MouseArea {
                    id: gearArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { shell.showSettings(); shell.hidePanel() }
                }
            }
        }

        Item {
            id: body
            objectName: "panelBody"
            x: panel.margin
            y: panel.margin + panel.headerHeight + panel.gap
            width: parent.width - 2 * panel.margin
            height: Math.max(0, panel.height - y - panel.gap - panel.footerHeight - panel.margin)
            clip: true

            ColumnLayout {
                id: col
                width: parent.width
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
                    UiText {
                        Layout.preferredWidth: S.sp(40)
                        horizontalAlignment: Text.AlignRight
                        text: player.volume + "%"
                        color: theme.dimForeground
                        px: 12
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
                        text: title + "  ·  " + subtitle
                        elide: Text.ElideRight
                    }
                }
            }
        }

        // footer
        Rectangle {
            objectName: "panelFooter"
            x: panel.margin
            y: parent.height - panel.margin - panel.footerHeight
            width: parent.width - 2 * panel.margin
            height: panel.footerHeight
            radius: S.sp(6)
            color: mainArea.containsMouse ? theme.selection : theme.surface
            UiText { anchors.centerIn: parent; text: qsTr("Open omamusic") }
            MouseArea {
                id: mainArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: { shell.showMain(); shell.hidePanel() }
            }
        }
    }
}
