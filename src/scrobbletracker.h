#pragma once

#include <QObject>

// Decides when a played server song is "now playing" and when it counts as played:
// once it has run half its length or four minutes, whichever comes first, and only
// when it is at least 30 seconds long. A new song, or a repeat of the same one
// after it ended, starts over.
class ScrobbleTracker : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    // The playing song's path (empty when stopped), its position and duration in seconds.
    void update(const QString &path, double position, double duration);

signals:
    void nowPlaying(const QString &id);   // a song started
    void submit(const QString &id);       // a song counts as played

private:
    static QString idOf(const QString &path);

    QString m_path;
    bool m_submitted = false;
    double m_last = 0;
};
