#pragma once

#include <QHash>
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
    Q_PROPERTY(int trackCount READ trackCount NOTIFY indexChanged)
    Q_PROPERTY(RowsModel *albums READ albums CONSTANT)
    Q_PROPERTY(RowsModel *artists READ artists CONSTANT)
    Q_PROPERTY(RowsModel *folder READ folder CONSTANT)
    Q_PROPERTY(RowsModel *songs READ songs CONSTANT)
    Q_PROPERTY(RowsModel *results READ results CONSTANT)
    Q_PROPERTY(QString folderPath READ folderPath NOTIFY folderPathChanged)

public:
    Library(const QStringList &roots, const QString &scanProgram, const QString &cacheFile,
            QObject *parent = nullptr);
    ~Library() override;

    void setTracks(QVector<Track> tracks);
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

    Q_INVOKABLE void rescan();
    // The known tracks for these paths, in order, each with "playlistIndex" set
    // to its position in `paths`; unknown paths are skipped.
    Q_INVOKABLE QVariantList tracksForPaths(const QStringList &paths) const;
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

private:
    void loadCache();
    void writeCache() const;
    void drainScanOutput(bool flush);
    void rebuildGroups();
    void setupWatch();
    QString coverCacheDir() const;
    QString cachedCoverPath(const QString &dir) const;
    void queueExtraction(const QString &dir, const QString &track);
    void startNextExtraction();
    void finishExtraction(bool ok);

    QStringList m_roots;
    QString m_scanProgram;
    QString m_cacheFile;

    QVector<Track> m_tracks;
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
};
