#pragma once

#include <QObject>
#include <QList>
#include <QMap>
#include <QStringList>

#include "rowsmodel.h"
#include "track.h"

// Playlists as plain .m3u files (one path per line, with any #EXTINF lines kept) in one directory. The name
// of a playlist is its file name without the extension.
class Playlists : public QObject
{
    Q_OBJECT
    Q_PROPERTY(RowsModel *list READ list CONSTANT)

public:
    explicit Playlists(const QString &dir = defaultDir(), QObject *parent = nullptr);

    static QString defaultDir();
    // A name usable as a file name: trimmed, no slashes or leading dots; empty if nothing is left.
    static QString cleanName(const QString &name);

    RowsModel *list() { return &m_list; }
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    int revision() const { return m_revision; }
    // The names of the playlists (local first, then the server's) that hold this song.
    Q_INVOKABLE QStringList namesFor(const QString &path) const { return m_membership.value(Track::normalized(path)); }

    // A playlist that lives on the server: read-only, listed after the local ones.
    struct ServerList
    {
        QString name;
        QStringList paths;
        QString id;                          // the server's id for the playlist
        QString serverId = QStringLiteral("s1");
        QString source = QStringLiteral("server");   // shown beside the playlist (the server's name)
        bool readOnly = true;
    };
    // The lists of one server; each server's lists are kept apart.
    void setServerLists(const QString &serverId, const QList<ServerList> &lists);
    void setServerLists(const QList<ServerList> &lists) { setServerLists(QStringLiteral("s1"), lists); }

    Q_INVOKABLE QStringList names() const;
    Q_INVOKABLE QStringList paths(const QString &name) const;
    // Each return value tells whether it did anything.
    Q_INVOKABLE bool create(const QString &name);
    Q_INVOKABLE bool remove(const QString &name);
    Q_INVOKABLE bool rename(const QString &name, const QString &newName);
    Q_INVOKABLE bool addPaths(const QString &name, const QStringList &paths);
    Q_INVOKABLE bool removeAt(const QString &name, int index);
    // Creates the playlist when needed (a unique name if it exists) and fills it.
    Q_INVOKABLE QString createWith(const QString &name, const QStringList &paths);

signals:
    void changed();

private:
    QString fileFor(const QString &name) const;
    // A song with the "#..." lines (such as #EXTINF) that precede it in the file.
    struct Entry
    {
        QString path;
        QStringList meta;
    };
    QList<Entry> entries(const QString &name) const;
    bool write(const QString &name, const QList<Entry> &entries);
    bool write(const QString &name, const QStringList &paths);
    void reload();

    QMap<QString, QList<ServerList>> m_server;   // server id -> its lists
    QHash<QString, QStringList> m_membership;   // song path -> playlist names
    int m_revision = 0;
    QString m_dir;
    RowsModel m_list;
};
