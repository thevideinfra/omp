import QtQuick
import QtQuick.Layouts
import "Scale.js" as S

// The YouTube Music area: your library as songs, as albums and as playlists. The search box above
// the window filters the songs and albums by title, artist and album.
Item {
    id: root
    property string section: "Library"   // Library, Albums or Playlists
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

    // The list the keyboard works on (the same calls as the other tabs).
    readonly property var current: stack.children[stack.currentIndex]

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Segmented {
            Layout.margins: S.sp(8)
            options: ["Library", "Albums", "Playlists"]
            selected: root.section
            onPicked: (value) => { root.section = value; root.clicked() }
        }

        StackLayout {
            id: stack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.section === "Albums" ? 1 : root.section === "Playlists" ? 2 : 0

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
        }
    }
}
