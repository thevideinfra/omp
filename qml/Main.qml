import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Fmt.js" as Fmt

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

    readonly property var tabNames: ["Albums", "Artists", "Folders", "Queue"]
    property int tab: 0
    readonly property bool searching: searchField.text.length > 0

    onClosing: (close) => { close.accepted = false; visible = false }   // close-to-tray

    function currentList() {
        return stack.children[stack.currentIndex]
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
        case Qt.Key_Tab: win.tab = (win.tab + 1) % 4; searchField.text = ""; break
        case Qt.Key_J:
        case Qt.Key_Down: currentList().move(1); break
        case Qt.Key_K:
        case Qt.Key_Up: currentList().move(-1); break
        case Qt.Key_Return:
        case Qt.Key_Enter: currentList().activate(); break
        case Qt.Key_A: currentList().enqueue(); break
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
                    Layout.preferredWidth: 140
                    color: theme.sunken

                    Column {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4
                        Repeater {
                            model: win.tabNames
                            delegate: Rectangle {
                                required property int index
                                required property string modelData
                                width: parent.width
                                height: 36
                                radius: 6
                                color: win.tab === index && !win.searching ? theme.selection : "transparent"
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    x: 10
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
                        Layout.margins: 8
                        spacing: 8

                        TextField {
                            id: searchField
                            Layout.fillWidth: true
                            placeholderText: qsTr("Search ( / )")
                            color: theme.foreground
                            placeholderTextColor: theme.muted
                            selectByMouse: true
                            background: Rectangle { radius: 6; color: theme.surface }
                            onTextChanged: library.setQuery(text)
                            Keys.onEscapePressed: { text = ""; keyRoot.forceActiveFocus() }
                            Keys.onReturnPressed: keyRoot.forceActiveFocus()
                        }

                        Rectangle {
                            visible: win.tab === 2 && !win.searching
                            Layout.preferredWidth: 60
                            Layout.preferredHeight: searchField.height
                            radius: 6
                            color: upArea.containsMouse ? theme.selection : theme.surface
                            Text { anchors.centerIn: parent; text: qsTr("Up"); color: theme.foreground }
                            MouseArea { id: upArea; anchors.fill: parent; hoverEnabled: true; onClicked: { library.up(); keyRoot.forceActiveFocus() } }
                        }

                        Rectangle {
                            visible: win.tab === 3 && !win.searching
                            Layout.preferredWidth: 110
                            Layout.preferredHeight: searchField.height
                            radius: 6
                            color: clearArea.containsMouse ? theme.selection : theme.surface
                            Text { anchors.centerIn: parent; text: qsTr("Clear queue"); color: theme.foreground }
                            MouseArea { id: clearArea; anchors.fill: parent; hoverEnabled: true; onClicked: { player.clearQueue(); keyRoot.forceActiveFocus() } }
                        }
                    }

                    StackLayout {
                        id: stack
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: win.searching ? 4 : win.tab

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
                Layout.preferredHeight: 84
                color: theme.sunken

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 14

                    Rectangle {
                        Layout.preferredWidth: 64
                        Layout.preferredHeight: 64
                        color: theme.surface
                        Image {
                            anchors.fill: parent
                            source: library.coverFor(player.current.path ?? "")
                            fillMode: Image.PreserveAspectCrop
                            sourceSize.width: 128
                            sourceSize.height: 128
                            asynchronous: true
                        }
                    }

                    Column {
                        Layout.preferredWidth: 220
                        Text {
                            width: parent.width
                            text: player.current.title ?? qsTr("Nothing playing")
                            color: theme.foreground
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: player.current.artist ?? ""
                            color: theme.dimForeground
                            elide: Text.ElideRight
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Row {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 8
                            TransportButton { glyph: "◀◀"; onClicked: { player.previous(); keyRoot.forceActiveFocus() } }
                            TransportButton { glyph: player.playing ? "❚❚" : "▶"; big: true; onClicked: { player.togglePause(); keyRoot.forceActiveFocus() } }
                            TransportButton { glyph: "▶▶"; onClicked: { player.next(); keyRoot.forceActiveFocus() } }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: Fmt.time(player.position); color: theme.dimForeground; font.pixelSize: 12 }
                            SeekBar {
                                Layout.fillWidth: true
                                value: player.duration > 0 ? player.position / player.duration : 0
                                onSeeked: (f) => { player.seekTo(f * player.duration); keyRoot.forceActiveFocus() }
                            }
                            Text { text: Fmt.time(player.duration); color: theme.dimForeground; font.pixelSize: 12 }
                        }
                    }

                    RowLayout {
                        Layout.preferredWidth: 150
                        Text { text: qsTr("vol"); color: theme.dimForeground; font.pixelSize: 12 }
                        SeekBar {
                            Layout.fillWidth: true
                            value: player.volume / 100
                            onSeeked: (f) => { player.volume = Math.round(f * 100); keyRoot.forceActiveFocus() }
                        }
                    }
                }
            }
        }
    }
}
