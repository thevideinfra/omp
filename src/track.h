#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

struct Track
{
    QString path;
    QString title;
    QString artist;
    QString albumArtist;
    QString album;
    int track = 0;
    int disc = 0;
    int year = 0;          // 0 when unknown
    double duration = 0;
    qint64 size = 0;
    qint64 mtime = 0;
    QString coverId;       // a server song's cover art id
    bool starred = false;  // a server song marked as a favorite
    QString albumId;       // a YouTube Music song's album, by its browse id (to play the whole album)

    bool isRemote() const { return isRemotePath(path); }

    // A server song's path is "subsonic:<server id>/<song id>". Before there were several servers it
    // was "subsonic:<song id>"; those belong to the first server, "s1". A YouTube Music song is
    // "ytm:<video id>" and belongs to the source "ytm".
    static bool isRemotePath(const QString &path)
    {
        return path.startsWith(QLatin1String("subsonic:")) || path.startsWith(QLatin1String("ytm:"));
    }
    static bool isYoutubePath(const QString &path) { return path.startsWith(QLatin1String("ytm:")); }
    static QString remotePath(const QString &serverId, const QString &songId);
    static QString serverOf(const QString &path);      // "" for a local path
    static QString songIdOf(const QString &path);
    static QString normalized(const QString &path);    // an old-style remote path in the new style

    static Track fromJsonLine(const QByteArray &line, bool *ok = nullptr);
    QByteArray toJsonLine() const;
    QVariantMap toMap() const;
    static QStringList roleNames();
    static QString formatTime(double seconds);
};
