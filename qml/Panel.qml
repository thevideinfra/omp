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
    // Sum of the visible children rather than col.implicitHeight: a positioner
    // only updates its own size when it is polished, which a hidden window never
    // is, so freeze() would read a stale value. Text.implicitHeight is current
    // even while hidden.
    readonly property real contentHeight: {
        let h = 0, n = 0
        for (const c of col.children) {
            const ch = c.px !== undefined ? c.implicitHeight : c.height   // UiText or explicit
            if (c.visible && ch > 0) { h += ch; n++ }
        }
        return h + Math.max(0, n - 1) * col.spacing
    }
    readonly property int wantedHeight: Math.max(120, Math.min(maxHeight,
        headerHeight + gap + contentHeight + gap + footerHeight + 2 * margin))
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
            border.color: Qt.alpha(theme.accent, 0.45)
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
                color: gearArea.containsMouse ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
                border.width: 1
                border.color: gearArea.containsMouse ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12)
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

            // Children must be UiText (single-line, elided) or have an explicit
            // height that does not depend on polish.
            Column {
                id: col
                width: parent.width
                spacing: S.sp(10)

                Rectangle {
                    visible: ui.panelCover
                    x: (col.width - width) / 2
                    width: S.sp(200)
                    height: S.sp(200)
                    color: theme.surface
                    Image {
                        anchors.fill: parent
                        source: { library.coverRevision; return library.coverFor(player.current.path ?? "") }
                        fillMode: Image.PreserveAspectCrop
                        sourceSize.width: 400
                        sourceSize.height: 400
                        asynchronous: true
                    }
                }

                UiText {
                    width: col.width
                    horizontalAlignment: Text.AlignHCenter
                    text: player.current.title ?? qsTr("Nothing playing")
                    px: 16
                    font.bold: true
                    elide: Text.ElideRight
                }
                UiText {
                    width: col.width
                    horizontalAlignment: Text.AlignHCenter
                    text: player.current.artist ?? ""
                    color: theme.dimForeground
                    elide: Text.ElideRight
                }

                Item {
                    visible: ui.panelSeek
                    width: col.width
                    height: Math.max(seekPos.implicitHeight, seekBar.implicitHeight, seekDur.implicitHeight)
                    UiText {
                        id: seekPos
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: Fmt.time(player.position); color: theme.dimForeground; px: 12
                    }
                    SeekBar {
                        id: seekBar
                        anchors.left: seekPos.right
                        anchors.right: seekDur.left
                        anchors.leftMargin: S.sp(8)
                        anchors.rightMargin: S.sp(8)
                        anchors.verticalCenter: parent.verticalCenter
                        value: player.duration > 0 ? player.position / player.duration : 0
                        onSeeked: (f) => player.seekTo(f * player.duration)
                    }
                    UiText {
                        id: seekDur
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: Fmt.time(player.duration); color: theme.dimForeground; px: 12
                    }
                }

                Row {
                    // Explicit size: a positioner's own size only updates in
                    // polish (see contentHeight).
                    x: (col.width - width) / 2
                    width: prevBtn.implicitWidth + playBtn.implicitWidth + nextBtn.implicitWidth + 2 * spacing
                    height: Math.max(prevBtn.implicitHeight, playBtn.implicitHeight, nextBtn.implicitHeight)
                    spacing: S.sp(8)
                    TransportButton { id: prevBtn; glyph: "◀◀"; onClicked: player.previous() }
                    TransportButton { id: playBtn; glyph: player.playing ? "❚❚" : "▶"; big: true; onClicked: player.togglePause() }
                    TransportButton { id: nextBtn; glyph: "▶▶"; onClicked: player.next() }
                }

                Row {
                    x: (col.width - width) / 2
                    width: shuffleBtn.implicitWidth + repeatBtn.implicitWidth + spacing
                    height: Math.max(shuffleBtn.implicitHeight, repeatBtn.implicitHeight)
                    spacing: S.sp(8)
                    TransportButton {
                        id: shuffleBtn
                        icon: "shuffle"
                        active: player.shuffle
                        onClicked: player.shuffle = !player.shuffle
                    }
                    TransportButton {
                        id: repeatBtn
                        icon: player.repeat === "one" ? "repeatOne" : "repeat"
                        active: player.repeat !== "off"
                        onClicked: player.cycleRepeat()
                    }
                }

                Item {
                    visible: ui.panelVolume
                    width: col.width
                    height: Math.max(volLabel.implicitHeight, volBar.implicitHeight, volPct.implicitHeight)
                    UiText {
                        id: volLabel
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("vol"); color: theme.dimForeground; px: 12
                    }
                    SeekBar {
                        id: volBar
                        anchors.left: volLabel.right
                        anchors.right: volPct.left
                        anchors.leftMargin: S.sp(8)
                        anchors.rightMargin: S.sp(8)
                        anchors.verticalCenter: parent.verticalCenter
                        value: player.volume / 100
                        onSeeked: (f) => player.volume = Math.round(f * 100)
                    }
                    UiText {
                        id: volPct
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: S.sp(40)
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
                // Only the upcoming rows get a delegate (at most 10), so a long
                // queue costs nothing.
                Repeater {
                    model: ui.panelUpNext
                        ? Math.max(0, Math.min(10, ui.panelUpNextCount,
                                               player.queue.count - player.currentIndex - 1))
                        : 0
                    delegate: UiText {
                        required property int index
                        // get() is not tracked: read what changes when rows change.
                        readonly property var row: {
                            player.current; player.queue.count
                            return player.queue.get(player.currentIndex + 1 + index)
                        }
                        width: col.width
                        text: (row.title ?? "") + "  ·  " + (row.subtitle ?? "")
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
            color: mainArea.containsMouse ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
            border.width: 1
            border.color: mainArea.containsMouse ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12)
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
