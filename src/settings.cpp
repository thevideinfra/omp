#include "settings.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QSaveFile>

#include "paths.h"
#include "theme.h"

namespace {

enum class Type { Enum, String, Bool, Int };

struct Key
{
    QString prop;      // QML / API name
    QString file;      // config.toml key
    Type type;
    QVariant def;
    QStringList options;
};

constexpr int kMaxUpNext = 10;

const QVector<Key> &keys()
{
    static const QVector<Key> k{
        {"density", "density", Type::Enum, QString("normal"), {"compact", "normal", "comfortable"}},
        {"fontSize", "font_size", Type::Enum, QString("normal"), {"small", "normal", "large"}},
        {"fontFamily", "font_family", Type::String, QString(), {}},
        {"accent", "accent", Type::String, QString("theme"), {}},
        {"panelCover", "panel_cover", Type::Bool, true, {}},
        {"panelSeek", "panel_seek", Type::Bool, true, {}},
        {"panelVolume", "panel_volume", Type::Bool, true, {}},
        {"panelUpNext", "panel_up_next", Type::Bool, true, {}},
        {"panelUpNextCount", "panel_up_next_count", Type::Int, 4, {}},
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
        if (k.prop == QLatin1String("accent") && s.isEmpty())
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
        return ok ? QVariant(qBound(0, n, kMaxUpNext)) : QVariant();
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
    return out;
}

QByteArray Settings::apply(const QByteArray &existing, const QVariantMap &updates)
{
    QList<QByteArray> lines = existing.split('\n');
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
    for (const Key &k : keys()) {
        if (updates.contains(k.prop) && !done.contains(k.prop))
            missing.append(formatLine(k, updates.value(k.prop)));
    }
    if (missing.isEmpty())
        return out;

    if (!out.isEmpty() && !out.endsWith('\n'))
        out += '\n';
    if (!out.contains("# omamusic settings")) {
        if (!out.isEmpty())
            out += '\n';
        out += "# omamusic settings\n";
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
