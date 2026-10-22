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
input=$(cat)
case "$cmd" in
  login)
    case "$input" in *curl*) printf '{"cookie": "c1"}\n' ;; *) echo '{"code":"other","error":"no cookie"}' >&2; exit 1 ;; esac ;;
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
    echo '{"kind":"playlist","id":"PL1","name":"Mix","paths":["ytm:bbb","ytm:aaa"]}'
    printf '{"kind":"progress","done":1,"total":1}' ;;
esac
)SH");
    f.close();
    QFile::setPermissions(path, QFile::permissions(path) | QFileDevice::ExeOwner);
    return path;
}

