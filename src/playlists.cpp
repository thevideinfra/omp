#include "playlists.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include "paths.h"
#include "track.h"

Playlists::Playlists(const QString &dir, QObject *parent)
    : QObject(parent)
    , m_dir(dir)
    , m_list({"key", "title", "subtitle", "meta", "cover", "readOnly", "serverId", "playlistId"})
{
    reload();
}

QString Playlists::defaultDir()
{
    return Paths::dataDir() + QStringLiteral("/playlists");
}

QString Playlists::cleanName(const QString &name)
{
    QString n = name.trimmed();
    n.replace(QLatin1Char('/'), QLatin1Char('-'));
    n.replace(QLatin1Char('\\'), QLatin1Char('-'));
    n.remove(QLatin1Char('\n'));
    while (n.startsWith(QLatin1Char('.')))
        n.remove(0, 1);
    return n.trimmed().left(100);
}

QString Playlists::fileFor(const QString &name) const
{
    return m_dir + QLatin1Char('/') + name + QStringLiteral(".m3u");
}

QStringList Playlists::names() const
{
    QStringList out;
    for (const QFileInfo &f : QDir(m_dir).entryInfoList({QStringLiteral("*.m3u")}, QDir::Files)) {
        out << f.completeBaseName();
    }
    out.sort(Qt::CaseInsensitive);
    return out;
}

QList<Playlists::Entry> Playlists::entries(const QString &name) const
{
    QList<Entry> out;
    QFile f(fileFor(name));
    if (!f.open(QIODevice::ReadOnly))
        return out;
    QStringList meta;
    for (const QByteArray &raw : f.readAll().split('\n')) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (line.isEmpty() || line == QLatin1String("#EXTM3U"))
            continue;
        if (line.startsWith(QLatin1Char('#'))) {
            meta << line;
        } else {
            out.append({line, meta});
            meta.clear();
        }
    }
    return out;
}

namespace {
// The row key of a server playlist: it cannot clash with the name of a local file.
const QString kServerKey = QStringLiteral("\x1f""server\x1f");
}

static QString serverKey(const Playlists::ServerList &s)
{
    return kServerKey + s.serverId + QLatin1Char('\x1f') + s.name;
}

void Playlists::setServerLists(const QString &serverId, const QList<ServerList> &lists)
{
    QList<ServerList> own = lists;
    for (ServerList &s : own)
        s.serverId = serverId;
    if (own.isEmpty())
        m_server.remove(serverId);
    else
        m_server.insert(serverId, own);
    reload();
}

QVariantList Playlists::editableServerLists() const
{
    QVariantList out;
    for (const QList<ServerList> &lists : m_server)
        for (const ServerList &s : lists)
            if (!s.readOnly)
                out << QVariantMap{{"name", s.name}, {"serverId", s.serverId}, {"playlistId", s.id}, {"source", s.source}};
    return out;
}

QStringList Playlists::paths(const QString &name) const
{
    QStringList out;
    if (name.startsWith(kServerKey)) {
        for (const QList<ServerList> &lists : std::as_const(m_server))
            for (const ServerList &s : lists)
                if (serverKey(s) == name)
                    return s.paths;
        return out;
    }
    for (const Entry &e : entries(name))
        out << e.path;
    return out;
}

bool Playlists::write(const QString &name, const QStringList &paths)
{
    QList<Entry> list;
    for (const QString &p : paths)
        list.append({p, {}});
    return write(name, list);
}

bool Playlists::write(const QString &name, const QList<Entry> &list)
{
    if (!QDir().mkpath(m_dir)) {
        qWarning() << "cannot create" << m_dir;
        return false;
    }
    QSaveFile f(fileFor(name));
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "cannot write playlist" << name << f.errorString();
        return false;
    }
    f.write("#EXTM3U\n");
    for (const Entry &e : list) {
        for (const QString &m : e.meta)
            f.write(m.toUtf8() + '\n');
        f.write(e.path.toUtf8() + '\n');
    }
    if (!f.commit()) {
        qWarning() << "cannot write playlist" << name;
        return false;
    }
    return true;
}

void Playlists::reload()
{
    m_membership.clear();
    for (const QString &n : names())
        for (const QString &p : paths(n))
            if (!m_membership[Track::normalized(p)].contains(n))
                m_membership[Track::normalized(p)] << n;
    for (const QList<ServerList> &lists : std::as_const(m_server))
        for (const ServerList &s : lists)
            for (const QString &p : s.paths)
                if (!m_membership[Track::normalized(p)].contains(s.name))
                    m_membership[Track::normalized(p)] << s.name;
    ++m_revision;
    QVector<QVariantMap> rows;
    for (const QString &n : names()) {
        const int count = paths(n).size();
        rows.append({{"key", n},
                     {"title", n},
                     {"subtitle", QStringLiteral("%1 songs").arg(count)},
                     {"meta", QStringLiteral("local")},
                     {"cover", QString()},
                     {"readOnly", false},
                     {"serverId", QString()},
                     {"playlistId", QString()}});
    }
    for (const QList<ServerList> &lists : std::as_const(m_server)) {
        for (const ServerList &s : lists) {
            rows.append({{"key", serverKey(s)},
                         {"title", s.name},
                         {"subtitle", QStringLiteral("%1 songs").arg(s.paths.size())},
                         {"meta", s.source},
                         {"cover", QString()},
                         {"readOnly", s.readOnly},
                         {"serverId", s.serverId},
                         {"playlistId", s.id}});
        }
    }
    m_list.setRows(rows);
    emit changed();
}

bool Playlists::create(const QString &name)
{
    const QString n = cleanName(name);
    if (n.isEmpty() || QFileInfo::exists(fileFor(n)) || !write(n, QStringList()))
        return false;
    reload();
    return true;
}

bool Playlists::remove(const QString &name)
{
    if (!QFile::remove(fileFor(name)))
        return false;
    reload();
    return true;
}

bool Playlists::rename(const QString &name, const QString &newName)
{
    const QString n = cleanName(newName);
    if (n.isEmpty() || n == name || QFileInfo::exists(fileFor(n)) || !QFile::rename(fileFor(name), fileFor(n)))
        return false;
    reload();
    return true;
}

bool Playlists::addPaths(const QString &name, const QStringList &add)
{
    if (!QFileInfo::exists(fileFor(name)) || add.isEmpty())
        return false;
    QList<Entry> all = entries(name);
    for (const QString &p : add)
        all.append({p, {}});
    if (!write(name, all))
        return false;
    reload();
    return true;
}

bool Playlists::removeAt(const QString &name, int index)
{
    QList<Entry> all = entries(name);
    if (index < 0 || index >= all.size())
        return false;
    all.removeAt(index);
    if (!write(name, all))
        return false;
    reload();
    return true;
}

QString Playlists::createWith(const QString &name, const QStringList &add)
{
    QString base = cleanName(name);
    if (base.isEmpty())
        return {};
    QString n = base;
    for (int i = 2; QFileInfo::exists(fileFor(n)); ++i)
        n = QStringLiteral("%1 (%2)").arg(base).arg(i);
    if (!write(n, add))
        return {};
    reload();
    return n;
}
