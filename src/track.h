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
    double duration = 0;
    qint64 size = 0;
    qint64 mtime = 0;

    static Track fromJsonLine(const QByteArray &line, bool *ok = nullptr);
    QByteArray toJsonLine() const;
    QVariantMap toMap() const;
    static QStringList roleNames();
    static QString formatTime(double seconds);
};
