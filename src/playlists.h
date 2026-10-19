#pragma once

#include <QObject>
#include <QStringList>

#include "rowsmodel.h"

// Playlists as plain .m3u files (one path per line) in one directory. The name
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
    bool write(const QString &name, const QStringList &paths);
    void reload();

    QString m_dir;
    RowsModel m_list;
};
