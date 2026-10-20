#include "settings.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QSaveFile>
#include <QUrl>

#include "config.h"
#include "paths.h"
#include "theme.h"

namespace {

constexpr int kMaxUpNext = 10;
constexpr int kMaxVolume = 130;

enum class Type { Enum, String, Bool, Int };

struct Key
{
    QString prop;      // QML / API name
    QString file;      // config.toml key
    Type type;
    QVariant def;
    QStringList options;
    int max = kMaxUpNext;   // upper bound of an Int key
};

const QVector<Key> &keys()
{
    static const QVector<Key> k{
        {"density", "density", Type::Enum, QString("normal"), {"compact", "normal", "comfortable"}},
        {"fontSize", "font_size", Type::Enum, QString("normal"), {"small", "normal", "large"}},
        {"fontFamily", "font_family", Type::String, QString(), {}},
        {"accent", "accent", Type::String, QString("theme"), {}},
        {"showAccent", "show_accent", Type::Bool, true, {}},
        {"panelDensity", "panel_density", Type::Enum, QString("normal"), {"compact", "normal", "comfortable"}},
        {"panelFontSize", "panel_font_size", Type::Enum, QString("normal"), {"small", "normal", "large"}},
        {"panelFontFamily", "panel_font_family", Type::String, QString(), {}},
        {"panelAccent", "panel_accent", Type::String, QString("theme"), {}},
        {"panelShowAccent", "panel_show_accent", Type::Bool, true, {}},
        {"panelCover", "panel_cover", Type::Bool, true, {}},
        {"panelSeek", "panel_seek", Type::Bool, true, {}},
        {"panelVolume", "panel_volume", Type::Bool, true, {}},
        {"panelUpNext", "panel_up_next", Type::Bool, true, {}},
        {"panelUpNextCount", "panel_up_next_count", Type::Int, 4, {}},
        {"volume", "volume", Type::Int, 100, {}, kMaxVolume},
        {"shuffle", "shuffle", Type::Bool, false, {}},
        {"repeat", "repeat", Type::Enum, QString("off"), {"off", "all", "one"}},
        {"eqPreset", "eq_preset", Type::String, QString("Flat"), {}},
        {"saveLayout", "save_layout", Type::Bool, false, {}},
        {"saveSort", "save_sort", Type::Bool, false, {}},
        {"artistAlbumsColumn", "artist_albums_column", Type::Bool, true, {}},
        {"hiddenTabs", "hidden_tabs", Type::String, QString(), {}},
        {"songColumns", "song_columns", Type::String, QString("title,artist,album,year,time"), {}},
        {"railCollapsed", "rail_collapsed", Type::Bool, false, {}},
        {"showSourceTags", "show_source_tags", Type::Bool, true, {}},
        {"serverEnabled", "server_enabled", Type::Bool, false, {}},
        {"serverUrl", "server_url", Type::String, QString(), {}},
        {"serverUser", "server_user", Type::String, QString(), {}},
        {"serverType", "server_type", Type::String, QString(), {}},
        {"sortSongs", "sort_songs", Type::String, QString(), {}},
        {"sortAlbums", "sort_albums", Type::String, QString(), {}},
        {"sortArtists", "sort_artists", Type::String, QString(), {}},
        {"layoutAlbum", "layout_album", Type::String, QString(), {}},
        {"layoutArtist", "layout_artist", Type::String, QString(), {}},
        {"layoutPlaylist", "layout_playlist", Type::String, QString(), {}},
    };
    return k;
}

const Key *findKey(const QString &prop)
{
    for (const Key &k : keys()) {
        if (k.prop == prop)
            return &k;
    }
    return nullptr;
}

// The song table's columns in their fixed order; "title" is always there.
const QStringList &allColumns()
{
    static const QStringList all{"title", "artist", "album", "albumartist", "year", "track", "added", "size",
                                 "format", "favorite", "playlists", "time"};
    return all;
}

QString normalizedColumns(const QString &list)
{
    const QStringList wanted = list.split(',', Qt::SkipEmptyParts);
    QStringList out;
    for (const QString &c : allColumns())
        if (c == QLatin1String("title") || wanted.contains(c.trimmed()))
            out << c;
    return out.join(',');
}

// The tabs of the left panel that can be hidden (Settings cannot), in rail order.
const QStringList &hideableTabs()
{
    static const QStringList all{"Albums", "Artists", "Songs", "Playlists", "Libraries", "Queue"};
    return all;
}

QString normalizedTabs(const QString &list)
{
    const QStringList wanted = list.split(',', Qt::SkipEmptyParts);
    QStringList out;
    for (const QString &t : hideableTabs())
        if (wanted.contains(t))
            out << t;
    if (out.size() >= hideableTabs().size())
        out.removeLast();   // one tab always stays
    return out.join(',');
}

// The acceptable value, or an invalid QVariant when v is not valid for the key.
QVariant validated(const Key &k, const QVariant &v)
{
    switch (k.type) {
    case Type::Enum: {
        const QString s = v.toString();
        return k.options.contains(s) ? QVariant(s) : QVariant();
    }
    case Type::String: {
        const QString s = v.toString().trimmed();
        if (s.contains('"') || s.contains('\\') || s.contains('\n'))
            return {};
        if (k.prop == QLatin1String("songColumns"))
            return normalizedColumns(s);
        if (k.prop == QLatin1String("hiddenTabs"))
            return normalizedTabs(s);
        if (k.prop == QLatin1String("serverUrl") && !s.isEmpty()
            && !s.startsWith(QLatin1String("http://")) && !s.startsWith(QLatin1String("https://")))
            return {};
        if ((k.prop == QLatin1String("accent") || k.prop == QLatin1String("panelAccent")) && s.isEmpty())
            return {};
        return s;
    }
    case Type::Bool: {
        if (v.metaType().id() == QMetaType::Bool)
            return v.toBool();
        const QString s = v.toString().toLower();
        if (s == QLatin1String("true"))
            return true;
        if (s == QLatin1String("false"))
            return false;
        return {};
    }
    case Type::Int: {
        bool ok = false;
        const int n = v.toInt(&ok);
        return ok ? QVariant(qBound(0, n, k.max)) : QVariant();
    }
    }
    return {};
}

QByteArray formatLine(const Key &k, const QVariant &v)
{
    QString value;
    switch (k.type) {
    case Type::Enum:
    case Type::String: value = QStringLiteral("\"%1\"").arg(v.toString()); break;
    case Type::Bool: value = v.toBool() ? QStringLiteral("true") : QStringLiteral("false"); break;
    case Type::Int: value = QString::number(v.toInt()); break;
    }
    return (k.file + QStringLiteral(" = ") + value).toUtf8();
}

bool isKeyLine(const QByteArray &trimmed, const QString &fileKey)
{
    const QByteArray key = fileKey.toUtf8();
    return trimmed.startsWith(key) && trimmed.mid(key.size()).trimmed().startsWith('=');
}

}

Settings::Settings(const QString &file, QObject *parent)
    : QObject(parent)
    , m_file(file)
{
    QFile f(m_file);
    m_values = parse(f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray());
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(300);
    connect(&m_saveTimer, &QTimer::timeout, this, &Settings::save);
}

Settings::~Settings()
{
    if (m_saveTimer.isActive()) {
        m_saveTimer.stop();
        save();
    }
}

QString Settings::defaultPath()
{
    return Paths::configDir() + QStringLiteral("/config.toml");
}

QVariantMap Settings::parse(const QByteArray &text)
{
    const QHash<QString, QString> kv = Theme::parse(text);
    QVariantMap out;
    for (const Key &k : keys()) {
        QVariant v;
        if (kv.contains(k.file))
            v = validated(k, kv.value(k.file));
        out.insert(k.prop, v.isValid() ? v : k.def);
    }
    out.insert(QStringLiteral("folders"), Config::parseFolders(text));
    return out;
}

QByteArray Settings::apply(const QByteArray &existing, const QVariantMap &updates)
{
    QList<QByteArray> lines = existing.split('\n');
    if (updates.contains(QStringLiteral("folders"))) {
        // Drop the old folders entry, which may span several lines.
        QList<QByteArray> kept;
        bool inArray = false;
        for (const QByteArray &line : std::as_const(lines)) {
            const QByteArray trimmed = line.trimmed();
            if (!inArray && isKeyLine(trimmed, QStringLiteral("folders"))) {
                // Only an array can span lines; "folders = x" is a single line.
                const QString value = QString::fromUtf8(trimmed.mid(trimmed.indexOf('=') + 1));
                inArray = Config::startsArray(value) && !Config::closesArray(value);
                continue;
            }
            if (inArray) {
                if (Config::closesArray(QString::fromUtf8(trimmed)))
                    inArray = false;
                continue;
            }
            kept.append(line);
        }
        lines = kept;
    }
    QSet<QString> done;
    for (QByteArray &line : lines) {
        const QByteArray trimmed = line.trimmed();
        for (const Key &k : keys()) {
            if (!updates.contains(k.prop))
                continue;
            if (isKeyLine(trimmed, k.file)) {
                line = formatLine(k, updates.value(k.prop));
                done.insert(k.prop);
                break;
            }
        }
    }
    QByteArray out = lines.join('\n');

    QList<QByteArray> missing;
    if (updates.contains(QStringLiteral("folders"))) {
        QStringList quoted;
        for (const QString &p : updates.value(QStringLiteral("folders")).toStringList())
            quoted << QStringLiteral("\"%1\"").arg(p);
        missing.append(QStringLiteral("folders = [%1]").arg(quoted.join(QStringLiteral(", "))).toUtf8());
    }
    for (const Key &k : keys()) {
        if (updates.contains(k.prop) && !done.contains(k.prop))
            missing.append(formatLine(k, updates.value(k.prop)));
    }
    if (missing.isEmpty())
        return out;

    if (!out.isEmpty() && !out.endsWith('\n'))
        out += '\n';
    if (!out.contains("# OMP settings") && !out.contains("# omamusic settings")) {   // the old marker counts
        if (!out.isEmpty())
            out += '\n';
        out += "# OMP settings\n";
    }
    for (const QByteArray &line : std::as_const(missing))
        out += line + '\n';
    return out;
}

double Settings::densityScaleFor(const QString &density)
{
    if (density == QLatin1String("compact"))
        return 0.8;
    if (density == QLatin1String("comfortable"))
        return 1.25;
    return 1.0;
}

double Settings::fontScaleFor(const QString &size)
{
    if (size == QLatin1String("small"))
        return 0.9;
    if (size == QLatin1String("large"))
        return 1.15;
    return 1.0;
}

void Settings::addFolder(const QString &path)
{
    QString given = path.trimmed();
    if (given.startsWith(QLatin1String("file:")))
        given = QUrl(given).toLocalFile();           // from a folder dialog
    else if (given == QLatin1String("~") || given.startsWith(QLatin1String("~/")))
        given = QDir::homePath() + given.mid(1);
    const QString dir = QDir::cleanPath(given);
    if (dir.isEmpty() || dir.contains('"') || dir.contains('\\') || dir.contains('\n') || !QFileInfo(dir).isDir())
        return;
    QStringList list = folders();
    if (list.contains(dir))
        return;
    list << dir;
    m_values.insert(QStringLiteral("folders"), list);
    m_dirty.insert(QStringLiteral("folders"));
    emit changed();
    m_saveTimer.start();
}

void Settings::removeFolder(const QString &path)
{
    QStringList list = folders();
    if (list.size() <= 1 || !list.removeOne(QDir::cleanPath(path)))
        return;
    m_values.insert(QStringLiteral("folders"), list);
    m_dirty.insert(QStringLiteral("folders"));
    emit changed();
    m_saveTimer.start();
}

void Settings::set(const QString &prop, const QVariant &value)
{
    const Key *k = findKey(prop);
    if (!k) {
        qWarning() << "unknown setting" << prop;
        return;
    }
    const QVariant v = validated(*k, value);
    if (!v.isValid()) {
        qWarning() << "invalid value for setting" << prop << value;
        return;
    }
    if (m_values.value(prop) == v)
        return;
    m_values.insert(prop, v);
    m_dirty.insert(prop);
    emit changed();
    m_saveTimer.start();
}

namespace {
QString layoutProp(const QString &tab)
{
    if (tab == QLatin1String("album") || tab == QLatin1String("artist") || tab == QLatin1String("playlist"))
        return QStringLiteral("layout") + tab.left(1).toUpper() + tab.mid(1);
    return {};
}
}

QString Settings::layoutFor(const QString &tab) const
{
    const QString prop = layoutProp(tab);
    return prop.isEmpty() ? QString() : m_values.value(prop).toString();
}

void Settings::setLayoutFor(const QString &tab, const QString &fractions)
{
    const QString prop = layoutProp(tab);
    if (prop.isEmpty())
        return;
    const QStringList parts = fractions.split(',');
    if (parts.size() > 2)
        return;
    for (const QString &p : parts) {
        bool ok = false;
        const double v = p.toDouble(&ok);
        if (!ok || v <= 0 || v >= 1)
            return;
    }
    set(prop, fractions);
}

QStringList Settings::hiddenTabs() const
{
    return m_values.value("hiddenTabs").toString().split(',', Qt::SkipEmptyParts);
}

bool Settings::tabShown(const QString &name) const
{
    return !hiddenTabs().contains(name);
}

void Settings::setTabShown(const QString &name, bool shown)
{
    if (!hideableTabs().contains(name))
        return;
    QStringList now = hiddenTabs();
    if (shown)
        now.removeAll(name);
    else if (!now.contains(name))
        now << name;
    set(QStringLiteral("hiddenTabs"), now.join(','));   // normalized by validation (never all six)
}

QStringList Settings::songColumns() const
{
    return m_values.value("songColumns").toString().split(',', Qt::SkipEmptyParts);
}

void Settings::setSongColumn(const QString &column, bool shown)
{
    if (!allColumns().contains(column))
        return;
    QStringList now = songColumns();
    if (shown && !now.contains(column))
        now << column;
    else if (!shown)
        now.removeAll(column);
    set(QStringLiteral("songColumns"), now.join(','));   // normalized by validation
}

void Settings::resetLayout()
{
    emit layoutReset();   // before the clearing, so open tabs do not store their sizes again
    for (const char *tab : {"album", "artist", "playlist"})
        set(layoutProp(QString::fromLatin1(tab)), QString());
}

QStringList Settings::fontFamilies() const
{
    return QFontDatabase::families();
}

void Settings::save()
{
    m_saveTimer.stop();
    if (m_dirty.isEmpty())
        return;
    QVariantMap updates;
    for (const QString &prop : std::as_const(m_dirty))
        updates.insert(prop, m_values.value(prop));
    const QSet<QString> pending = m_dirty;
    m_dirty.clear();
    // A failed write leaves the keys dirty so the next save retries them.
    auto fail = [&] { m_dirty.unite(pending); };

    QByteArray existing;
    QFile in(m_file);
    if (in.open(QIODevice::ReadOnly))
        existing = in.readAll();

    if (!QDir().mkpath(QFileInfo(m_file).absolutePath())) {
        qWarning() << "cannot create settings directory for" << m_file;
        fail();
        return;
    }
    QSaveFile out(m_file);
    if (!out.open(QIODevice::WriteOnly)) {
        qWarning() << "cannot write settings" << m_file << out.errorString();
        fail();
        return;
    }
    out.write(apply(existing, updates));
    if (!out.commit()) {
        qWarning() << "cannot write settings" << m_file << out.errorString();
        fail();
    }
}
