#!/bin/bash
# usage: test_sigterm.sh path/to/omp
# Starts the app offscreen with temp dirs, signals it, and checks that it exits
# promptly and leaves no mpv behind. Exit 77 = skipped (no mpv).
app=$1
command -v mpv >/dev/null || { echo "mpv not found, skipping"; exit 77; }
command -v pgrep >/dev/null || { echo "pgrep not found, skipping"; exit 77; }

root=$(mktemp -d /tmp/omr.XXXX)
trap 'pkill -f -- "--input-ipc-server=$root/" 2>/dev/null; rm -rf "$root"' EXIT
mkdir -p "$root/run" "$root/cfg" "$root/state" "$root/music"
chmod 700 "$root/run"
export XDG_RUNTIME_DIR=$root/run XDG_CONFIG_HOME=$root/cfg XDG_STATE_HOME=$root/state
export HOME=$root QT_QPA_PLATFORM=offscreen OMAMUSIC_SCAN=/bin/true
pattern="--input-ipc-server=$root/"

run_case() {
    local sig=$1 pid i
    "$app" show >/dev/null 2>&1 &
    pid=$!
    for i in $(seq 50); do pgrep -f -- "$pattern" >/dev/null && break; sleep 0.1; done
    pgrep -f -- "$pattern" >/dev/null || { echo "FAIL($sig): mpv never started"; return 1; }
    kill -"$sig" "$pid"
    for i in $(seq 30); do kill -0 "$pid" 2>/dev/null || break; sleep 0.1; done
    if kill -0 "$pid" 2>/dev/null; then echo "FAIL($sig): app still running after 3 s"; kill -KILL "$pid"; return 1; fi
    wait "$pid" 2>/dev/null
    for i in $(seq 30); do pgrep -f -- "$pattern" >/dev/null || break; sleep 0.1; done
    if pgrep -f -- "$pattern" >/dev/null; then echo "FAIL($sig): mpv left running"; return 1; fi
    echo "ok($sig)"
}

rc=0
for sig in TERM INT KILL; do run_case $sig || rc=1; done
exit $rc
