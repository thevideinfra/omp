import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Scale.js" as S

// The YouTube Music area: your library as songs, as artists, as albums and as playlists. The search box above
// the window filters the songs and albums by title, artist and album.
Item {
    id: root
    property string section: "Library"   // Library, Artists, Albums, Playlists or Search
    signal clicked()
    signal columnsRequested(real x, real y)
    signal pickRequested(var tracks)
    signal contextRequested(var tracks, real x, real y, bool canRemove)

    // A click on a column header: the first click sorts ascending, the next on the same one descending.
    function sortBy(key) {
        if (library.separateSort === key) {
            library.separateDescending = !library.separateDescending
        } else {
            library.separateSort = key
            library.separateDescending = false
        }
    }

    // The list the keyboard works on (the same calls as the other tabs); in Search, the songs found.
    readonly property var current: root.section === "Search" ? foundSongs : stack.children[stack.currentIndex]

    // Where an album's menu was asked for: the songs arrive a moment later.
    property real menuX: 0
    property real menuY: 0
    Connections {
        target: ytm
        function onAlbumFetched(id, purpose, tracks) {
            if (tracks.length === 0) return
            if (purpose === "play") player.playTracks(tracks, 0)
            else if (purpose === "queue") player.enqueueTracks(tracks)
            else if (purpose === "pick") root.pickRequested(tracks)
            else if (purpose === "menu") root.contextRequested(tracks, root.menuX, root.menuY, false)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Segmented {
            Layout.margins: S.sp(8)
            options: ["Library", "Artists", "Albums", "Playlists", "Search"]
            selected: root.section
            onPicked: (value) => { root.section = value; root.clicked() }
        }

        StackLayout {
            id: stack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: ["Library", "Artists", "Albums", "Playlists", "Search"].indexOf(root.section)

            BrowseList {   // the songs, in the order YouTube gives them until a header is clicked
                onClicked: root.clicked()
                songRows: true
                table: true
                columns: settingsStore.songColumns
                onColumnsRequested: (x, y) => root.columnsRequested(x, y)
                sortable: true
                sortKey: library.separateSort
                sortDescending: library.separateDescending
                onSortRequested: (key) => root.sortBy(key)
                model: library.separateSongs
                onActivated: (row) => player.playTracks([library.separateSongs.get(row)], 0)
                onEnqueued: (row) => player.enqueue(library.separateSongs.get(row))
                onMenuRequested: (row) => root.pickRequested([library.separateSongs.get(row)])
                onContextRequested: (row, x, y) => root.contextRequested([library.separateSongs.get(row)], x, y, false)
            }

            DrillList {
                kind: "artist"
                source: "ytm"
                model: library.separateArtists
                onClicked: root.clicked()
                onPickRequested: (tracks) => root.pickRequested(tracks)
                onContextRequested: (tracks, x, y, canRemove) => root.contextRequested(tracks, x, y, canRemove)
            }

            DrillList {
                kind: "album"
                source: "ytm"
                model: library.separateAlbums
                onClicked: root.clicked()
                onPickRequested: (tracks) => root.pickRequested(tracks)
                onContextRequested: (tracks, x, y, canRemove) => root.contextRequested(tracks, x, y, canRemove)
            }

            DrillList {
                kind: "playlist"
                allowNew: false
                model: playlists.listOf("ytm")
                onClicked: root.clicked()
                onPickRequested: (tracks) => root.pickRequested(tracks)
                onContextRequested: (tracks, x, y, canRemove) => root.contextRequested(tracks, x, y, canRemove)
            }

            ColumnLayout {   // songs and albums from all of YouTube Music, not only your library
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: S.sp(8)
                    spacing: S.sp(8)
                    TextField {
                        id: query
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search YouTube Music for songs and albums")
                        color: theme.foreground
                        placeholderTextColor: theme.muted
                        selectByMouse: true
                        font.pixelSize: S.fs(13)
                        font.family: ui.fontFamily !== "" ? ui.fontFamily : Qt.application.font.family
                        background: Rectangle { radius: S.sp(6); color: theme.surface }
                        onAccepted: { ytm.search(text); root.clicked() }
                        Keys.onEscapePressed: root.clicked()
                    }
                    ChipButton { text: qsTr("Search"); enabled: query.text.trim() !== "" && !ytm.searching; onClicked: { ytm.search(query.text); root.clicked() } }
                }

                UiText {
                    visible: ytm.searchStatus !== ""
                    Layout.leftMargin: S.sp(12)
                    Layout.bottomMargin: S.sp(6)
                    text: ytm.searchStatus
                    color: theme.dimForeground
                    px: 12
                }

                SplitView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    orientation: Qt.Horizontal

                    ColumnLayout {
                        SplitView.fillWidth: true
                        SplitView.minimumWidth: S.sp(240)
                        spacing: 0
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: S.sp(30)
                            color: theme.sunken
                            UiText { x: S.sp(12); anchors.verticalCenter: parent.verticalCenter; text: qsTr("Songs") + "  (" + foundSongs.count + ")"; color: theme.dimForeground; px: 12 }
                        }
                        BrowseList {
                            id: foundSongs
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            onClicked: root.clicked()
                            songRows: true
                            model: ytm.searchSongs
                            onActivated: (row) => player.playTracks([ytm.searchSongs.get(row)], 0)
                            onEnqueued: (row) => player.enqueue(ytm.searchSongs.get(row))
                            onMenuRequested: (row) => root.pickRequested([ytm.searchSongs.get(row)])
                            onContextRequested: (row, x, y) => root.contextRequested([ytm.searchSongs.get(row)], x, y, false)
                        }
                    }

                    ColumnLayout {
                        SplitView.preferredWidth: S.sp(340)
                        SplitView.minimumWidth: S.sp(220)
                        spacing: 0
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: S.sp(30)
                            color: theme.sunken
                            UiText { x: S.sp(12); anchors.verticalCenter: parent.verticalCenter; text: qsTr("Albums") + "  (" + foundAlbums.count + ")"; color: theme.dimForeground; px: 12 }
                        }
                        BrowseList {
                            id: foundAlbums
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            onClicked: root.clicked()
                            artSize: S.sp(72)
                            model: ytm.searchAlbums
                            onActivated: (row) => ytm.fetchAlbum(ytm.searchAlbums.get(row).key, "play")
                            onEnqueued: (row) => ytm.fetchAlbum(ytm.searchAlbums.get(row).key, "queue")
                            onMenuRequested: (row) => ytm.fetchAlbum(ytm.searchAlbums.get(row).key, "pick")
                            onContextRequested: (row, x, y) => { root.menuX = x; root.menuY = y; ytm.fetchAlbum(ytm.searchAlbums.get(row).key, "menu") }
                        }
                    }
                }
            }
        }
    }
}