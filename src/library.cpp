#include "library.h"

#include <algorithm>

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSaveFile>
#include <QSet>
#include <QUrl>

namespace {

QString albumKey(const Track &t)
{
    return t.albumArtist + QChar(0x1f) + t.album;
}

bool trackLess(const Track &a, const Track &b)
{
    if (a.disc != b.disc)
        return a.disc < b.disc;
    if (a.track != b.track)
        return a.track < b.track;
    return a.path < b.path;
}

QVariantList toList(QVector<Track> tracks)
{
    std::sort(tracks.begin(), tracks.end(), trackLess);
    QVariantList out;
    out.reserve(tracks.size());
    for (const Track &t : tracks)
        out << t.toMap();
    return out;
}

QString findCover(const QString &dir)
{
    static const QStringList names{"cover.jpg", "cover.png", "folder.jpg",
                                   "folder.png", "front.jpg", "front.png"};
    const QStringList files = QDir(dir).entryList(QDir::Files);
    for (const QString &name : names) {
        for (const QString &file : files) {
            if (file.compare(name, Qt::CaseInsensitive) == 0)
                return QUrl::fromLocalFile(dir + '/' + file).toString();
        }
    }
    return {};
}

QStringList folderRoles()
{
    QStringList roles = Track::roleNames();
    roles << "isDir";
    return roles;
}

QVariantMap dirRow(const QString &path, const QString &name)
{
    return {{"path", path}, {"title", name}, {"subtitle", QString()}, {"meta", QString()},
            {"cover", QString()}, {"isDir", true}};
}

bool ciLess(const QString &a, const QString &b)
{
    return QString::compare(a, b, Qt::CaseInsensitive) < 0;
}

}

Library::Library(const QStringList &roots, const QString &scanProgram, const QString &cacheFile,
                 QObject *parent)
    : QObject(parent)
    , m_roots(roots)
    , m_scanProgram(scanProgram)
    , m_cacheFile(cacheFile)
    , m_albums({"key", "title", "subtitle", "meta", "cover"})
    , m_artists({"key", "title", "subtitle", "meta", "cover"})
    , m_folder(folderRoles())
    , m_results(Track::roleNames())
{
    for (QString &r : m_roots)
        r = QDir::cleanPath(r);

    m_scan.setProgram(m_scanProgram);
    connect(&m_scan, &QProcess::readyReadStandardOutput, this, [this] {
        m_scanBuffer += m_scan.readAllStandardOutput();
        drainScanOutput(false);
    });
    connect(&m_scan, &QProcess::finished, this,
            [this](int code, QProcess::ExitStatus status) {
                drainScanOutput(true);
                if (status == QProcess::NormalExit && code == 0 && m_pending.isEmpty() && !m_tracks.isEmpty()) {
                    // An unmounted share scans clean but empty; do not wipe the index for it.
                    qWarning() << "scan found no tracks, keeping old index";
                } else if (status == QProcess::NormalExit && code == 0) {
                    setTracks(m_pending);
                    writeCache();
                } else {
                    qWarning() << "omamusic-scan failed, keeping old index; exit code" << code;
                }
                m_pending.clear();
                emit scanningChanged();
            });
    connect(&m_scan, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            qWarning() << "cannot start" << m_scanProgram;
            emit scanningChanged();
        }
    });

    m_extractTimer.setSingleShot(true);
    m_extractTimer.setInterval(15000);
    connect(&m_extractTimer, &QTimer::timeout, &m_extract, &QProcess::kill);
    connect(&m_extract, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        finishExtraction(status == QProcess::NormalExit && code == 0);
    });
    connect(&m_extract, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            if (!m_ffmpegMissing) {
                m_ffmpegMissing = true;
                qWarning() << "cannot start ffmpeg for embedded cover art";
            }
            m_extractQueue.clear();   // no point trying the remaining dirs
            finishExtraction(false);
        }
    });
}

Library::~Library()
{
    m_extract.disconnect(this);
    if (m_extract.state() != QProcess::NotRunning) {
        m_extract.kill();
        m_extract.waitForFinished(1000);
    }
    m_scan.disconnect(this);
    if (m_scan.state() != QProcess::NotRunning) {
        m_scan.kill();
        m_scan.waitForFinished(1000);
    }
}

void Library::start()
{
    loadCache();
    browse(QString());
    rescan();
}

void Library::rescan()
{
    if (m_scan.state() != QProcess::NotRunning)
        return;
    m_pending.clear();
    m_scanBuffer.clear();
    QStringList args{"--cache", m_cacheFile};
    args += m_roots;
    m_scan.setArguments(args);
    m_scan.start();
    emit scanningChanged();
}

void Library::drainScanOutput(bool flush)
{
    int nl;
    while ((nl = m_scanBuffer.indexOf('\n')) >= 0) {
        bool ok = false;
        const Track t = Track::fromJsonLine(m_scanBuffer.left(nl), &ok);
        if (ok)
            m_pending.append(t);
        m_scanBuffer.remove(0, nl + 1);
    }
    if (flush && !m_scanBuffer.trimmed().isEmpty()) {
        bool ok = false;
        const Track t = Track::fromJsonLine(m_scanBuffer, &ok);
        if (ok)
            m_pending.append(t);
        m_scanBuffer.clear();
    }
}

void Library::loadCache()
{
    QFile f(m_cacheFile);
    if (!f.open(QIODevice::ReadOnly))
        return;
    QVector<Track> tracks;
    while (!f.atEnd()) {
        bool ok = false;
        const Track t = Track::fromJsonLine(f.readLine(), &ok);
        if (ok)
            tracks.append(t);
    }
    if (!tracks.isEmpty())
        setTracks(tracks);
}

void Library::writeCache() const
{
    if (!QDir().mkpath(QFileInfo(m_cacheFile).absolutePath())) {
        qWarning() << "cannot create index cache directory for" << m_cacheFile;
        return;
    }
    QSaveFile f(m_cacheFile);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "cannot write index cache" << m_cacheFile << f.errorString();
        return;
    }
    for (const Track &t : m_tracks) {
        f.write(t.toJsonLine());
        f.write("\n");
    }
    if (!f.commit())
        qWarning() << "cannot write index cache (commit failed)" << m_cacheFile << f.errorString();
}

void Library::setTracks(QVector<Track> tracks)
{
    m_tracks = std::move(tracks);
    m_coverCache.clear();
    rebuildGroups();
    browse(m_folderPath);
    setQuery(m_query);
    emit indexChanged();
}

void Library::rebuildGroups()
{
    m_byAlbum.clear();
    m_byArtist.clear();
    for (const Track &t : std::as_const(m_tracks)) {
        m_byAlbum[albumKey(t)].append(t);
        m_byArtist[t.albumArtist].append(t);
    }

    struct Album { QString key, name, artist; QVector<Track> tracks; };
    QVector<Album> albums;
    for (auto it = m_byAlbum.cbegin(); it != m_byAlbum.cend(); ++it) {
        QVector<Track> sorted = it.value();
        std::sort(sorted.begin(), sorted.end(), trackLess);
        albums.append({it.key(), sorted.first().album, sorted.first().albumArtist, sorted});
    }
    std::sort(albums.begin(), albums.end(), [](const Album &a, const Album &b) {
        const int byArtist = QString::compare(a.artist, b.artist, Qt::CaseInsensitive);
        return byArtist != 0 ? byArtist < 0 : ciLess(a.name, b.name);
    });
    QVector<QVariantMap> albumRows;
    for (const Album &a : std::as_const(albums)) {
        albumRows.append({{"key", a.key},
                          {"title", a.name},
                          {"subtitle", a.artist},
                          {"meta", QStringLiteral("%1 tracks").arg(a.tracks.size())},
                          {"cover", a.tracks.first().path}});   // resolved lazily by coverFor()
    }
    m_albums.setRows(albumRows);

    QStringList names = m_byArtist.keys();
    std::sort(names.begin(), names.end(), ciLess);
    QVector<QVariantMap> artistRows;
    for (const QString &name : std::as_const(names)) {
        QSet<QString> albumNames;
        for (const Track &t : m_byArtist.value(name))
            albumNames.insert(t.album);
        artistRows.append({{"key", name},
                           {"title", name},
                           {"subtitle", QStringLiteral("%1 albums").arg(albumNames.size())},
                           {"meta", QStringLiteral("%1 tracks").arg(m_byArtist.value(name).size())},
                           {"cover", QString()}});
    }
    m_artists.setRows(artistRows);
}

QVariantList Library::albumTracks(const QString &key) const
{
    return toList(m_byAlbum.value(key));
}

QVariantList Library::artistTracks(const QString &key) const
{
    QVector<Track> tracks = m_byArtist.value(key);
    std::sort(tracks.begin(), tracks.end(), [](const Track &a, const Track &b) {
        if (a.album != b.album)
            return ciLess(a.album, b.album);
        return trackLess(a, b);
    });
    QVariantList out;
    for (const Track &t : std::as_const(tracks))
        out << t.toMap();
    return out;
}

void Library::browse(const QString &dir)
{
    QVector<QVariantMap> rows;
    if (dir.isEmpty()) {
        for (const QString &root : std::as_const(m_roots)) {
            const QString name = QFileInfo(root).fileName();
            rows.append(dirRow(root, name.isEmpty() ? root : name));
        }
    } else {
        const QString prefix = dir.endsWith('/') ? dir : dir + '/';
        QMap<QString, QString> dirs;
        QVector<Track> files;
        for (const Track &t : std::as_const(m_tracks)) {
            if (!t.path.startsWith(prefix))
                continue;
            const QString rest = t.path.mid(prefix.size());
            const int slash = rest.indexOf('/');
            if (slash < 0)
                files.append(t);
            else
                dirs.insert(rest.left(slash), prefix + rest.left(slash));
        }
        for (auto it = dirs.cbegin(); it != dirs.cend(); ++it)
            rows.append(dirRow(it.value(), it.key()));
        std::sort(files.begin(), files.end(),
                  [](const Track &a, const Track &b) { return a.path < b.path; });
        for (const Track &t : std::as_const(files)) {
            QVariantMap row = t.toMap();
            row.insert("isDir", false);
            rows.append(row);
        }
    }
    m_folder.setRows(rows);
    if (m_folderPath != dir) {
        m_folderPath = dir;
        emit folderPathChanged();
    }
}

void Library::up()
{
    if (m_folderPath.isEmpty())
        return;
    if (m_roots.contains(m_folderPath))
        browse(QString());
    else
        browse(QFileInfo(m_folderPath).path());
}

void Library::setQuery(const QString &query)
{
    m_query = query;
    const QStringList terms = query.toLower().split(' ', Qt::SkipEmptyParts);
    QVector<QVariantMap> rows;
    if (!terms.isEmpty()) {
        for (const Track &t : std::as_const(m_tracks)) {
            const QString hay = (t.title + ' ' + t.artist + ' ' + t.album).toLower();
            const bool all = std::all_of(terms.cbegin(), terms.cend(),
                                         [&](const QString &term) { return hay.contains(term); });
            if (all) {
                rows.append(t.toMap());
                if (rows.size() >= 500)
                    break;
            }
        }
    }
    m_results.setRows(rows);
}

QString Library::coverFor(const QString &trackPath)
{
    if (trackPath.isEmpty())
        return QString();
    const QString dir = QFileInfo(trackPath).absolutePath();
    auto it = m_coverCache.constFind(dir);
    if (it != m_coverCache.cend())
        return it.value();
    QString found = findCover(dir);
    if (found.isEmpty()) {
        const QString cached = cachedCoverPath(dir);
        if (QFileInfo::exists(cached))
            found = QUrl::fromLocalFile(cached).toString();
        else
            queueExtraction(dir, trackPath);
    }
    m_coverCache.insert(dir, found);
    return found;
}

QString Library::coverCacheDir() const
{
    return QFileInfo(m_cacheFile).absolutePath() + QStringLiteral("/covers");
}

QString Library::cachedCoverPath(const QString &dir) const
{
    const QByteArray hex = QCryptographicHash::hash(dir.toUtf8(), QCryptographicHash::Sha1).toHex();
    return coverCacheDir() + '/' + QString::fromLatin1(hex) + QStringLiteral(".jpg");
}

void Library::queueExtraction(const QString &dir, const QString &track)
{
    if (m_ffmpegMissing || m_extractSeen.contains(dir))
        return;
    m_extractSeen.insert(dir);
    if (!QDir().mkpath(coverCacheDir()))
        return;   // read-only state dir: no extraction
    m_extractQueue.enqueue({dir, track});
    if (m_extract.state() == QProcess::NotRunning)
        startNextExtraction();
}

void Library::startNextExtraction()
{
    if (m_extract.state() != QProcess::NotRunning)
        return;   // re-entered via a slot: a run is already in flight
    if (m_extractQueue.isEmpty())
        return;
    const auto [dir, track] = m_extractQueue.dequeue();
    m_extractDir = dir;
    const QString tmp = cachedCoverPath(dir).chopped(4) + QStringLiteral(".tmp.jpg");
    QFile::remove(tmp);
    m_extract.start(QStringLiteral("ffmpeg"),
                    {"-v", "error", "-i", track, "-an", "-frames:v", "1", "-y", tmp});
    if (m_extract.state() != QProcess::NotRunning)   // sync FailedToStart already finished
        m_extractTimer.start();
}

void Library::finishExtraction(bool ok)
{
    m_extractTimer.stop();
    const QString finalPath = cachedCoverPath(m_extractDir);
    const QString tmp = finalPath.chopped(4) + QStringLiteral(".tmp.jpg");
    if (ok && QFileInfo(tmp).size() > 0) {
        QFile::remove(finalPath);
        if (QFile::rename(tmp, finalPath)) {
            m_coverCache.insert(m_extractDir, QUrl::fromLocalFile(finalPath).toString());
            ++m_coverRevision;
            emit coversChanged();
        }
    } else {
        QFile::remove(tmp);   // no embedded art, or ffmpeg failed: not retried this session
    }
    startNextExtraction();
}
