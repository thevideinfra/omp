#pragma once

#include <QHash>
#include <functional>

#include <QMap>
#include <QObject>
#include <QProcess>
#include <QFileSystemWatcher>
#include <QQueue>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QVector>

#include "rowsmodel.h"
#include "track.h"

class Library : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int coverRevision READ coverRevision NOTIFY coversChanged)
    Q_PROPERTY(int starredRevision READ starredRevision NOTIFY starredChanged)
    Q_PROPERTY(int trackCount READ trackCount NOTIFY indexChanged)
    Q_PROPERTY(RowsModel *albums READ albums CONSTANT)
    Q_PROPERTY(RowsModel *artists READ artists CONSTANT)
    Q_PROPERTY(RowsModel *folder READ folder CONSTANT)
    Q_PROPERTY(RowsModel *songs READ songs CONSTANT)
    Q_PROPERTY(RowsModel *results READ results CONSTANT)
    Q_PROPERTY(QString folderPath READ folderPath NOTIFY folderPathChanged)
    // Folders the last scan could not reach (unmounted or hung shares); their songs are the last known ones.
    // How the Songs list is ordered: "title", "artist", "album", "albumartist", "year", "track", "length", "size", "format", "added" (newest first) or "favorite" (starred first).
    Q_PROPERTY(QString songSort READ songSort WRITE setSongSort NOTIFY songSortChanged)
    Q_PROPERTY(bool songDescending READ songDescending WRITE setSongDescending NOTIFY songSortChanged)
    // Albums: "title", "artist" (the default), "length" or "added"; artists: "title", "length" or "added".
    Q_PROPERTY(QString albumSort READ albumSort WRITE setAlbumSort NOTIFY albumSortChanged)
    Q_PROPERTY(bool albumDescending READ albumDescending WRITE setAlbumDescending NOTIFY albumSortChanged)
    Q_PROPERTY(QString artistSort READ artistSort WRITE setArtistSort NOTIFY artistSortChanged)
    Q_PROPERTY(bool artistDescending READ artistDescending WRITE setArtistDescending NOTIFY artistSortChanged)
    Q_PROPERTY(QStringList offlineRoots READ offlineRoots NOTIFY offlineChanged)

public:
    Library(const QStringList &roots, const QString &scanProgram, const QString &cacheFile,
            QObject *parent = nullptr);
    ~Library() override;

    // The local songs (the scanned index).
    void setTracks(QVector<Track> tracks);
    // The songs of a Subsonic server, listed beside the local ones; they never appear in the folder tree.
    // The songs of one server ("s1" is the first); each server's songs and cache stay apart.
    void setRemoteTracks(const QString &serverId, QVector<Track> tracks);
    void setRemoteTracks(QVector<Track> tracks) { setRemoteTracks(QStringLiteral("s1"), std::move(tracks)); }
    void loadRemoteCache(const QString &serverId = QStringLiteral("s1"));
    // The server songs marked as favorites, in title order.
    QStringList starredRemotePaths() const;
    Q_INVOKABLE bool isStarred(const QString &path) const;
    // What the song table shows beyond the row itself: albumArtist, track, disc, year, mtime, size, format.
    Q_INVOKABLE QVariantMap trackInfo(const QString &path) const;
    // The keys of the rows that stand for this song's artist and album in the Artists and Albums lists.
    Q_INVOKABLE QString artistKeyFor(const QString &path) const;
    Q_INVOKABLE QString albumKeyFor(const QString &path) const;
    Q_INVOKABLE int yearFor(const QString &path) const { return m_byPath.value(Track::normalized(path)).year; }
    // Marks a server song as a favorite (or not) in place: the lists are not rebuilt.
    void setStarred(const QString &path, bool starred);
    int starredRevision() const { return m_starredRevision; }
    // Asks for a server cover to be downloaded to a file; returns false when it cannot
    // right now (no server settings yet), so the library asks again later.
    void setCoverFetcher(std::function<bool(const QString &serverId, const QString &coverId, const QString &destFile)> fetcher)
    {
        m_coverFetcher = std::move(fetcher);
    }
    // The download for a cover id finished (the file is in place).
    void coverFetched(const QString &coverId);
    void writeRemoteCache(const QString &serverId = QStringLiteral("s1")) const;
    void start();
    // Replaces the music folders and rescans (after the running scan, if any).
    void setRoots(const QStringList &roots);

    bool scanning() const { return m_scan.state() != QProcess::NotRunning; }
    int trackCount() const { return m_tracks.size(); }
    RowsModel *albums() { return &m_albums; }
    RowsModel *artists() { return &m_artists; }
    RowsModel *folder() { return &m_folder; }
    RowsModel *songs() { return &m_songs; }
    RowsModel *results() { return &m_results; }
    QString folderPath() const { return m_folderPath; }
    // Treats every folder as reachable without scanning (for tests).
    void assumeReachable() { m_online = m_roots; }
    // A tab's sort as "key,asc" or "key,desc" (tab: "songs", "albums" or "artists"), for storing.
    QString sortState(const QString &tab) const;
    void restoreSortState(const QString &tab, const QString &state);
    QString songSort() const { return m_songSort; }
    void setSongSort(const QString &key);
    bool songDescending() const { return m_songDescending; }
    void setSongDescending(bool on);
    QString albumSort() const { return m_albumSort; }
    void setAlbumSort(const QString &key);
    bool albumDescending() const { return m_albumDescending; }
    void setAlbumDescending(bool on);
    QString artistSort() const { return m_artistSort; }
    void setArtistSort(const QString &key);
    bool artistDescending() const { return m_artistDescending; }
    void setArtistDescending(bool on);
    QStringList offlineRoots() const { return m_offline; }

    Q_INVOKABLE void rescan();
    // The known tracks for these paths, in order, each with "playlistIndex" set
    // to its position in `paths`; unknown paths are skipped.
    Q_INVOKABLE QVariantList tracksForPaths(const QStringList &paths) const;
    // The local songs under a folder (subfolders included), in path order.
    Q_INVOKABLE QVariantList tracksUnder(const QString &dir) const;
    Q_INVOKABLE QVariantList albumTracks(const QString &key) const;
    Q_INVOKABLE QVariantList artistTracks(const QString &key) const;
    Q_INVOKABLE void browse(const QString &dir);
    Q_INVOKABLE void up();
    Q_INVOKABLE void setQuery(const QString &query);
    Q_INVOKABLE QString coverFor(const QString &trackPath);
    // Whether changes under the roots are noticed through directory watches
    // (otherwise the library rescans on a timer).
    bool watching() const { return m_watching; }
    void setWatchDelay(int ms) { m_rescanTimer.setInterval(ms); }
    void setMaxWatchedDirs(int n) { m_maxWatchedDirs = n; }
    int coverRevision() const { return m_coverRevision; }
    bool extractingCovers() const { return m_extract.state() != QProcess::NotRunning || !m_extractQueue.isEmpty(); }

signals:
    void scanningChanged();
    void indexChanged();
    void folderPathChanged();
    void coversChanged();
    void offlineChanged();
    void songSortChanged();
    void starredChanged();
    void albumSortChanged();
    void artistSortChanged();

private:
    void loadCache();
    void writeCache() const;
    void drainScanOutput(bool flush);
    void rebuildGroups();
    void setupWatch();
    bool isOnline(const QString &path) const;
    void finishScan();
    void rebuildSongs();
    void combine();
    QString remoteCacheFile(const QString &serverId) const;
    void rebuildAlbums();
    void rebuildArtists();
    QString coverCacheDir() const;
    QString cachedCoverPath(const QString &dir) const;
    void queueExtraction(const QString &dir, const QString &track);
    void startNextExtraction();
    void finishExtraction(bool ok);

    QStringList m_roots;
    QString m_scanProgram;
    QString m_cacheFile;

    QVector<Track> m_local;    // scanned from the music folders
    QMap<QString, QVector<Track>> m_remote;   // server id -> its songs
    QVector<Track> m_tracks;   // both, what the lists are built from
    QHash<QString, QVector<Track>> m_byAlbum;    // album key -> tracks
    QHash<QString, Track> m_byPath;
    QHash<QString, QVector<Track>> m_byArtist;   // album artist -> tracks
    QHash<QString, QString> m_coverCache;           // album dir -> cover URL ("" = none yet)
    QQueue<QPair<QString, QString>> m_extractQueue; // (album dir, track path)
    QSet<QString> m_extractSeen;                    // dirs queued or already failed
    QProcess m_extract;
    QTimer m_extractTimer;
    QString m_extractDir;
    bool m_ffmpegMissing = false;
    int m_coverRevision = 0;
    int m_starredRevision = 0;
    std::function<bool(const QString &, const QString &, const QString &)> m_coverFetcher;
    QSet<QString> m_remoteCoversAsked;   // cover ids already requested this run
    QString remoteCoverFile(const QString &serverId, const QString &coverId) const;

    RowsModel m_albums;
    RowsModel m_artists;
    RowsModel m_folder;
    RowsModel m_songs;
    RowsModel m_results;
    QString m_folderPath;
    QString m_query;

    QFileSystemWatcher m_watcher;
    QTimer m_rescanTimer;   // debounce after a directory change
    QTimer m_pollTimer;     // fallback when the roots cannot be watched
    bool m_watching = false;
    bool m_dirty = false;   // a change arrived while a scan was running
    int m_maxWatchedDirs = 4096;

    QProcess m_scan;
    QByteArray m_scanBuffer;
    QVector<Track> m_pending;
    QString m_albumSort = QStringLiteral("artist");
    bool m_albumDescending = false;
    QString m_artistSort = QStringLiteral("title");
    bool m_artistDescending = false;
    QHash<QString, QString> m_artistDisplay;   // folded album artist -> the spelling shown
    QString m_songSort = QStringLiteral("title");
    bool m_songDescending = false;
    QStringList m_pendingOffline;
    QStringList m_offline;   // roots the last finished scan could not reach
    QStringList m_online;    // roots a finished scan reached; only these are touched on disk
};
