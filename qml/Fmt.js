.pragma library

function time(seconds) {
    if (!(seconds > 0))
        return "0:00"
    const s = Math.floor(seconds)
    return Math.floor(s / 60) + ":" + String(s % 60).padStart(2, "0")
}
