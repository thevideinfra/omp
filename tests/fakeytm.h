#pragma once

#include <QFile>
#include <QString>

// A stand-in for omp-ytm: `login` hands back the login, `sync` prints two songs and a playlist;
// FAKE_YTM_MODE picks "ok", "auth", "missing" or "crash" for sync.
inline QString writeHelper(const QString &dir)
{
    const QString path = dir + "/omp-ytm";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return {};
    f.write(R"SH(#!/bin/sh
cmd="$1"
[ "$cmd" = browsers ] && { echo '[{"label":"Brave Origin","spec":"browser:brave:/p/Default"},{"label":"Firefox","spec":"browser:firefox:/p/ff"}]'; exit 0; }
input=$(cat)
case "$cmd" in
  login)
    case "$input" in *curl*) printf '{"cookie": "c1"}\n' ;; *) echo '{"code":"other","error":"no cookie"}' >&2; exit 1 ;; esac ;;
  artist)
    case "$FAKE_YTM_MODE" in crash) echo boom >&2; exit 1 ;; esac
    echo "$2" > "$FAKE_YTM_DIR/artistid"
    echo '{"kind":"artist","name":"Zed","description":"Plays things.","cover_id":"https://x/z=w544-h544"}'
    echo '{"kind":"track","path":"ytm:t1","title":"Top One","artist":"Zed","album_artist":"Zed","album":"Gamma","duration":200,"artist_id":"UCzed"}'
    echo '{"kind":"track","path":"ytm:t2","title":"Top Two","artist":"Zed","album_artist":"Zed","album":"Gamma","duration":180,"artist_id":"UCzed"}'
    echo '{"kind":"album","id":"MPREb_g","title":"Gamma","artist":"Zed","year":2019,"type":"Album","section":"albums","cover_id":"https://x/g=w544-h544"}'
    printf '{"kind":"album","id":"MPREb_t","title":"Tiny","artist":"Zed","year":2021,"type":"Single","section":"singles"}' ;;
  radio)
    case "$FAKE_YTM_MODE" in crash) echo boom >&2; exit 1 ;; esac
    echo "$2 $3" > "$FAKE_YTM_DIR/radio"
    echo '{"kind":"track","path":"ytm:'"$2"'","title":"Seed","artist":"Zed","album_artist":"Zed","album":"Gamma","duration":201}'
    printf '{"kind":"track","path":"ytm:r1","title":"Similar","artist":"Yan","album_artist":"Yan","album":"Omega","duration":100}' ;;
  history)
    case "$FAKE_YTM_MODE" in crash) echo boom >&2; exit 1 ;; esac
    echo called >> "$FAKE_YTM_DIR/history-calls"
    echo '{"kind":"track","path":"ytm:h1","title":"Heard Today","artist":"Zed","album_artist":"Zed","album":"Gamma","duration":201,"cover_id":"https://x/h=w544-h544"}'
    printf '{"kind":"track","path":"ytm:h2","title":"Heard Yesterday","artist":"Yan","album_artist":"Yan","album":"Omega","duration":100}' ;;
  search)
    case "$FAKE_YTM_MODE" in auth) echo '{"code":"auth","error":"signed out"}' >&2; exit 2 ;; crash) echo boom >&2; exit 1 ;; esac
    echo "$2" > "$FAKE_YTM_DIR/query"
    echo "$3" > "$FAKE_YTM_DIR/limit"
    echo '{"kind":"track","path":"ytm:s1","title":"Found One","artist":"Zed","album_artist":"Zed","album":"Gamma","duration":215,"cover_id":"https://x/s=w544-h544"}'
    echo '{"kind":"track","path":"ytm:s2","title":"Found Two","artist":"Yan","album_artist":"Yan","album":"Omega","duration":100}'
    echo '{"kind":"album","id":"MPREb_g","title":"Gamma","artist":"Zed","year":2019,"type":"Album","cover_id":"https://x/a=w544-h544"}'
    printf '{"kind":"album","id":"MPREb_o","title":"Omega","artist":"Yan","year":0,"type":"Single"}' ;;
  album)
    case "$FAKE_YTM_MODE" in crash) echo boom >&2; exit 1 ;; esac
    echo "$2" > "$FAKE_YTM_DIR/albumid"
    echo '{"kind":"track","path":"ytm:g1","title":"Gamma One","artist":"Zed","album_artist":"Zed","album":"Gamma","track":1,"duration":200}'
    printf '{"kind":"track","path":"ytm:g2","title":"Gamma Two","artist":"Zed","album_artist":"Zed","album":"Gamma","track":2,"duration":210}' ;;
  playlist-create|playlist-add|playlist-remove|playlist-rename|playlist-delete|like|unlike|like-album)
    case "$FAKE_YTM_MODE" in crash) echo boom >&2; exit 1 ;; esac
    [ "$FAKE_YTM_EDITS" = fail ] && { echo '{"code":"other","error":"refused"}' >&2; exit 1; }
    shift
    echo "$cmd $*" >> "$FAKE_YTM_DIR/edits"
    case "$cmd" in
      playlist-create) shift; out='{"ok":true,"id":"PLnew","set_ids":{' ;;
      playlist-add) shift; out='{"ok":true,"set_ids":{' ;;
      *) echo '{"ok":true}'; exit 0 ;;
    esac
    sep=""
    for v in "$@"; do out="$out$sep\"$v\":\"set-$v\""; sep=","; done
    echo "$out}}" ;;
  sync)
    case "$FAKE_YTM_MODE" in
      auth) echo '{"code":"auth","error":"signed out"}' >&2; exit 2 ;;
      missing) echo '{"code":"missing","error":"python-ytmusicapi is not installed"}' >&2; exit 3 ;;
      crash) echo 'boom' >&2; exit 1 ;;
    esac
    echo "login seen: $input" > "$FAKE_YTM_DIR/seen"
    echo '{"kind":"progress","done":0,"total":1}'
    echo '{"kind":"track","path":"ytm:aaa","title":"One","artist":"Zed","album_artist":"Zed","album":"Gamma","duration":200,"cover_id":"https://x/a=w544-h544"}'
    echo '{"kind":"track","path":"ytm:bbb","title":"Two","artist":"Zed","album_artist":"Zed","album":"Gamma","duration":100}'
    echo '{"kind":"playlist","id":"PL1","name":"Mix","paths":["ytm:bbb","ytm:aaa"],"set_ids":["sv-b","sv-a"],"editable":true}'
    echo '{"kind":"playlist","id":"LM","name":"Liked Music","paths":["ytm:aaa"],"set_ids":[""],"editable":false}'
    printf '{"kind":"progress","done":1,"total":1}' ;;
esac
)SH");
    f.close();
    QFile::setPermissions(path, QFile::permissions(path) | QFileDevice::ExeOwner);
    return path;
}

