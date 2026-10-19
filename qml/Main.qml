import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Fmt.js" as Fmt
import "Scale.js" as S

ApplicationWindow {
    id: win
    objectName: "mainWindow"
    title: "omamusic"
    width: 1000
    height: 640
    minimumWidth: 520
    minimumHeight: 360
    visible: false
    color: theme.background

    readonly property var tabNames: ["Albums", "Artists", "Folders", "Queue", "Settings"]
    property int tab: 0
    readonly property bool searching: searchField.text.length > 0

    onClosing: (close) => { close.accepted = false; visible = false }   // close-to-tray

    function currentList() {
        return stack.children[stack.currentIndex]
    }

    // Called by Shell::showSettings() (the panel's gear).
    function openSettings() {
        win.tab = tabNames.indexOf("Settings")
        searchField.text = ""
        keyRoot.forceActiveFocus()
    }

    function handleKey(e) {
        if (e.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) {
            e.accepted = false
            return
        }
        e.accepted = true
        switch (e.key) {
        case Qt.Key_Space: player.togglePause(); break
        case Qt.Key_N: player.next(); break
        case Qt.Key_P: player.previous(); break
        case Qt.Key_H: player.seekBy(-10); break
        case Qt.Key_L: player.seekBy(10); break
        case Qt.Key_Slash: searchField.forceActiveFocus(); break
        case Qt.Key_Tab: win.tab = (win.tab + 1) % win.tabNames.length; searchField.text = ""; break
        case Qt.Key_J:
        case Qt.Key_Down: currentList().move(1); break
        case Qt.Key_K:
        case Qt.Key_Up: currentList().move(-1); break
        case Qt.Key_Return:
        case Qt.Key_Enter: currentList().activate(); break
        case Qt.Key_A: currentList().enqueue(); break
        case Qt.Key_R: library.rescan(); break
        case Qt.Key_Backspace:
            if (win.tab === 2 && !win.searching)
                library.up()
            break
        case Qt.Key_Escape: searchField.text = ""; break
        default: e.accepted = false
        }
    }

    Item {
        id: keyRoot
        anchors.fill: parent
        focus: true
        Keys.onPressed: (e) => win.handleKey(e)

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // left rail
                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: S.sp(140)
                    color: theme.sunken

                    Column {
                        anchors.fill: parent
                        anchors.margins: S.sp(8)
                        spacing: S.sp(4)
                        Repeater {
                            model: win.tabNames
                            delegate: Rectangle {
                                required property int index
                                required property string modelData
                                width: parent.width
                                height: S.sp(36)
                                radius: S.sp(6)
                                color: win.tab === index && !win.searching ? Qt.alpha(theme.accent, 0.12) : "transparent"
                                border.width: win.tab === index && !win.searching ? 1 : 0
                                border.color: Qt.alpha(theme.accent, 0.45)
                                UiText {
                                    anchors.verticalCenter: parent.verticalCenter
                                    x: S.sp(10)
                                    text: modelData
                                    color: win.tab === index ? theme.accent : theme.foreground
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: { win.tab = index; searchField.text = ""; keyRoot.forceActiveFocus() }
                                }
                            }
                        }
                    }
                }

                // content
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: S.sp(8)
                        spacing: S.sp(8)

                        TextField {
                            id: searchField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Search ( / )")
                            color: theme.foreground
                            placeholderTextColor: theme.muted
                            selectByMouse: true
                            font.pixelSize: S.fs(13)
                            font.family: ui.fontFamily !== "" ? ui.fontFamily : Qt.application.font.family
                            background: Rectangle { radius: S.sp(6); color: theme.surface }
                            onTextChanged: library.setQuery(text)
                            Keys.onEscapePressed: { text = ""; keyRoot.forceActiveFocus() }
                            Keys.onReturnPressed: keyRoot.forceActiveFocus()
                        }

                        Rectangle {
                            visible: win.tab === 2 && !win.searching
                            Layout.preferredWidth: S.sp(60)
                            Layout.preferredHeight: searchField.height
                            radius: S.sp(6)
                            color: upArea.containsMouse ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
                            border.width: 1
                            border.color: upArea.containsMouse ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12)
                            UiText { anchors.centerIn: parent; text: qsTr("Up") }
                            MouseArea { id: upArea; anchors.fill: parent; hoverEnabled: true; onClicked: { library.up(); keyRoot.forceActiveFocus() } }
                        }

                        Rectangle {
                            visible: win.tab === 3 && !win.searching
                            Layout.preferredWidth: S.sp(110)
                            Layout.preferredHeight: searchField.height
                            radius: S.sp(6)
                            color: clearArea.containsMouse ? Qt.alpha(theme.accent, 0.12) : Qt.alpha(theme.foreground, 0.05)
                            border.width: 1
                            border.color: clearArea.containsMouse ? Qt.alpha(theme.accent, 0.45) : Qt.alpha(theme.foreground, 0.12)
                            UiText { anchors.centerIn: parent; text: qsTr("Clear queue") }
                            MouseArea { id: clearArea; anchors.fill: parent; hoverEnabled: true; onClicked: { player.clearQueue(); keyRoot.forceActiveFocus() } }
                        }
                    }

                    StackLayout {
                        id: stack
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: win.searching ? 5 : win.tab

                        BrowseList {
                            onClicked: keyRoot.forceActiveFocus()   // Albums
                            model: library.albums
                            onActivated: (row) => player.playTracks(library.albumTracks(library.albums.get(row).key), 0)
                            onEnqueued: (row) => player.enqueueTracks(library.albumTracks(library.albums.get(row).key))
                        }

                        BrowseList {
                            onClicked: keyRoot.forceActiveFocus()   // Artists
                            model: library.artists
                            onActivated: (row) => player.playTracks(library.artistTracks(library.artists.get(row).key), 0)
                            onEnqueued: (row) => player.enqueueTracks(library.artistTracks(library.artists.get(row).key))
                        }

                        BrowseList {
                            onClicked: keyRoot.forceActiveFocus()   // Folders
                            id: folderList
                            model: library.folder
                            Connections {
                                target: library
                                function onFolderPathChanged() { folderList.currentIndex = 0 }
                            }
                            onActivated: (row) => {
                                const rows = library.folder.all()
                                const r = rows[row]
                                if (r.isDir) {
                                    library.browse(r.path)
                                    return
                                }
                                const tracks = rows.filter(x => !x.isDir)
                                player.playTracks(tracks, tracks.findIndex(x => x.path === r.path))
                            }
                            onEnqueued: (row) => {
                                const r = library.folder.get(row)
                                if (!r.isDir)
                                    player.enqueue(r)
                            }
                        }

                        BrowseList {
                            onClicked: keyRoot.forceActiveFocus()   // Queue
                            model: player.queue
                            playingRow: player.currentIndex
                            onActivated: (row) => player.jumpTo(row)
                        }

                        SettingsForm { }   // Settings

                        BrowseList {
                            onClicked: keyRoot.forceActiveFocus()   // Search results
                            model: library.results
                            onActivated: (row) => player.playTracks(library.results.all(), row)
                            onEnqueued: (row) => player.enqueue(library.results.get(row))
                        }
                    }
                }
            }

            // bottom playback bar
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: S.sp(84)
                color: theme.sunken

                UiText {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.rightMargin: S.sp(10)
                    anchors.topMargin: S.sp(2)
                    px: 11
                    color: theme.dimForeground
                    elide: Text.ElideRight
                    width: parent.width - S.sp(20)
                    horizontalAlignment: Text.AlignRight
                    text: player.error !== "" ? player.error
                          : library.scanning ? qsTr("Scanning…") : ""
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: S.sp(10)
                    spacing: S.sp(14)

                    Rectangle {
                        Layout.preferredWidth: S.sp(64)
                        Layout.preferredHeight: S.sp(64)
                        color: theme.surface
                        Image {
                            anchors.fill: parent
                            source: { library.coverRevision; return library.coverFor(player.current.path ?? "") }
                            fillMode: Image.PreserveAspectCrop
                            sourceSize.width: 128
                            sourceSize.height: 128
                            asynchronous: true
                        }
                    }

                    Column {
                        Layout.preferredWidth: S.sp(220)
                        UiText {
                            width: parent.width
                            text: player.current.title ?? qsTr("Nothing playing")
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        UiText {
                            width: parent.width
                            text: player.current.artist ?? ""
                            color: theme.dimForeground
                            elide: Text.ElideRight
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: S.sp(2)
                        Row {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: S.sp(8)
                            TransportButton { glyph: "◀◀"; onClicked: { player.previous(); keyRoot.forceActiveFocus() } }
                            TransportButton { glyph: player.playing ? "❚❚" : "▶"; big: true; onClicked: { player.togglePause(); keyRoot.forceActiveFocus() } }
                            TransportButton { glyph: "▶▶"; onClicked: { player.next(); keyRoot.forceActiveFocus() } }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            UiText { text: Fmt.time(player.position); color: theme.dimForeground; px: 12 }
                            SeekBar {
                                Layout.fillWidth: true
                                value: player.duration > 0 ? player.position / player.duration : 0
                                onSeeked: (f) => { player.seekTo(f * player.duration); keyRoot.forceActiveFocus() }
                            }
                            UiText { text: Fmt.time(player.duration); color: theme.dimForeground; px: 12 }
                        }
                    }

                    RowLayout {
                        Layout.preferredWidth: S.sp(200)
                        UiText { text: qsTr("vol"); color: theme.dimForeground; px: 12 }
                        SeekBar {
                            Layout.fillWidth: true
                            value: player.volume / 100
                            onSeeked: (f) => { player.volume = Math.round(f * 100); keyRoot.forceActiveFocus() }
                        }
                        UiText {
                            Layout.preferredWidth: S.sp(40)
                            horizontalAlignment: Text.AlignRight
                            text: player.volume + "%"
                            color: theme.dimForeground
                            px: 12
                        }
                    }
                }
            }
        }
    }
}
