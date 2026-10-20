#pragma once

#include <QFile>
#include <QString>

// A stand-in for secret-tool that keeps one file per account.
inline QString writeFake(const QString &dir)
{
    const QString path = dir + "/secret-tool";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return {};
    f.write(R"SH(#!/bin/sh
store="$FAKE_SECRET_DIR"
cmd="$1"; shift
account=""
while [ $# -gt 0 ]; do
  if [ "$1" = "account" ]; then account="$2"; fi
  shift
done
file="$store/$(printf %s "$account" | tr '/:@' '___')"
case "$cmd" in
  store) cat > "$file" ;;
  lookup) [ -f "$file" ] && cat "$file" || exit 1 ;;
  clear) rm -f "$file" ;;
esac
)SH");
    f.close();
    QFile::setPermissions(path, QFile::permissions(path) | QFileDevice::ExeOwner);
    return path;
}

