#include "track.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

Track Track::fromJsonLine(const QByteArray &line, bool *ok)
{
    const QJsonObject o = QJsonDocument::fromJson(line).object();
    Track t;
    t.path = o.value("path").toString();
    if (ok)
        *ok = !t.path.isEmpty();
    if (t.path.isEmpty())
        return t;
    t.title = o.value("title").toString();
    if (t.title.isEmpty())
        t.title = QFileInfo(t.path).completeBaseName();
    t.artist = o.value("artist").toString();
    if (t.artist.isEmpty())
        t.artist = QStringLiteral("Unknown Artist");
    t.albumArtist = o.value("album_artist").toString();
    if (t.albumArtist.isEmpty())
        t.albumArtist = t.artist;
    t.album = o.value("album").toString();
    if (t.album.isEmpty())
        t.album = QStringLiteral("Unknown Album");
    t.track = o.value("track").toInt();
    t.disc = o.value("disc").toInt();
    t.duration = o.value("duration").toDouble();
    t.size = o.value("size").toInteger();
    t.mtime = o.value("mtime").toInteger();
    t.coverId = o.value("cover_id").toString();
    t.starred = o.value("starred").toBool();
    return t;
}

QByteArray Track::toJsonLine() const
{
    QJsonObject o{
        {"path", path},
        {"title", title},
        {"artist", artist},
        {"album_artist", albumArtist},
        {"album", album},
        {"track", track},
        {"disc", disc},
        {"duration", duration},
        {"size", size},
        {"mtime", mtime},
    };
    if (!coverId.isEmpty())
        o.insert("cover_id", coverId);
    if (starred)
        o.insert("starred", true);
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

QVariantMap Track::toMap() const
{
    return {
        {"path", path},
        {"title", title},
        {"artist", artist},
        {"albumArtist", albumArtist},
        {"album", album},
        {"track", track},
        {"disc", disc},
        {"duration", duration},
        {"subtitle", artist + QStringLiteral(" — ") + album},
        {"meta", formatTime(duration)},
        {"coverId", coverId},
        {"starred", starred},
        {"remote", isRemote()},
        {"cover", path},   // a song shows the art of its folder; resolved lazily by Library::coverFor()
    };
}

QStringList Track::roleNames()
{
    return {"path", "title", "artist", "albumArtist", "album", "track",
            "disc", "duration", "subtitle", "meta", "cover", "coverId", "starred", "remote"};
}

QString Track::formatTime(double seconds)
{
    const int s = qRound(seconds);
    return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
}
