#pragma once

#include <QHash>
#include <QObject>
#include <QProcess>
#include <QVariantList>
#include <QVector>

#include "rowsmodel.h"
#include "track.h"

class Library : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int trackCount READ trackCount NOTIFY indexChanged)
    Q_PROPERTY(RowsModel *albums READ albums CONSTANT)
    Q_PROPERTY(RowsModel *artists READ artists CONSTANT)
    Q_PROPERTY(RowsModel *folder READ folder CONSTANT)
    Q_PROPERTY(RowsModel *results READ results CONSTANT)
    Q_PROPERTY(QString folderPath READ folderPath NOTIFY folderPathChanged)

public:
    Library(const QStringList &roots, const QString &scanProgram, const QString &cacheFile,
            QObject *parent = nullptr);
    ~Library() override;

    void setTracks(QVector<Track> tracks);
    void start();

    bool scanning() const { return m_scan.state() != QProcess::NotRunning; }
    int trackCount() const { return m_tracks.size(); }
    RowsModel *albums() { return &m_albums; }
    RowsModel *artists() { return &m_artists; }
    RowsModel *folder() { return &m_folder; }
    RowsModel *results() { return &m_results; }
    QString folderPath() const { return m_folderPath; }

    Q_INVOKABLE void rescan();
    Q_INVOKABLE QVariantList albumTracks(const QString &key) const;
    Q_INVOKABLE QVariantList artistTracks(const QString &key) const;
    Q_INVOKABLE void browse(const QString &dir);
    Q_INVOKABLE void up();
    Q_INVOKABLE void setQuery(const QString &query);
    Q_INVOKABLE QString coverFor(const QString &trackPath) const;

signals:
    void scanningChanged();
    void indexChanged();
    void folderPathChanged();

private:
    void loadCache();
    void writeCache() const;
    void drainScanOutput(bool flush);
    void rebuildGroups();

    QStringList m_roots;
    QString m_scanProgram;
    QString m_cacheFile;

    QVector<Track> m_tracks;
    QHash<QString, QVector<Track>> m_byAlbum;    // album key -> tracks
    QHash<QString, QVector<Track>> m_byArtist;   // album artist -> tracks
    mutable QHash<QString, QString> m_coverCache;

    RowsModel m_albums;
    RowsModel m_artists;
    RowsModel m_folder;
    RowsModel m_results;
    QString m_folderPath;
    QString m_query;

    QProcess m_scan;
    QByteArray m_scanBuffer;
    QVector<Track> m_pending;
};
