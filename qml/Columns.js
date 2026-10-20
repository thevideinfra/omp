.pragma library

// The columns a song table can show, in table order. `fixed` columns have a set width
// (in sp units), the others share the rest by `weight`. `sort` is the Library sort key a
// click on the header uses ("" = not sortable).
var order = ["title", "artist", "album", "albumartist", "year", "track", "added", "size", "format", "playlists", "time"]

var catalog = {
    title:       { label: "Title",        weight: 2.0, sort: "title" },
    artist:      { label: "Artist",       weight: 1.2, sort: "artist" },
    album:       { label: "Album",        weight: 1.2, sort: "album" },
    albumartist: { label: "Album artist", weight: 1.0, sort: "albumartist" },
    year:        { label: "Year",         fixed: 56,   sort: "year" },
    track:       { label: "#",            fixed: 48,   sort: "track" },
    added:       { label: "Added",        fixed: 104,  sort: "added" },
    size:        { label: "Size",         fixed: 80,   sort: "size" },
    format:      { label: "Format",       fixed: 72,   sort: "format" },
    playlists:   { label: "Playlists",    weight: 1.2, sort: "" },
    time:        { label: "Time",         fixed: 64,   sort: "length", right: true }
}
