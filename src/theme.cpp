#include "theme.h"

#include <QFile>
#include <QFileInfo>
#include <QStringList>

#include "paths.h"

namespace {

const QStringList &accentNames()
{
    static const QStringList names{"blue", "cyan", "green", "magenta", "yellow", "red", "orange"};
    return names;
}

QString shellFileFor(const QString &colorsFile)
{
    return QFileInfo(colorsFile).absolutePath() + QStringLiteral("/shell.toml");
}

const QHash<QString, QColor> &fallback()
{
    static const QHash<QString, QColor> colors{
        {"background", QColor("#1e1e2e")},   {"surface", QColor("#313244")},
        {"sunken", QColor("#181825")},       {"foreground", QColor("#cdd6f4")},
        {"dimForeground", QColor("#a6adc8")}, {"accent", QColor("#89b4fa")},
        {"selection", QColor("#45475a")},    {"muted", QColor("#6c7086")},
        {"danger", QColor("#f38ba8")},
    };
    return colors;
}

// colors.toml key -> Theme property
const QHash<QString, QString> &keyMap()
{
    static const QHash<QString, QString> map{
        {"background", "background"},           {"lighter_background", "surface"},
        {"dark_background", "sunken"},          {"foreground", "foreground"},
        {"dark_foreground", "dimForeground"},   {"accent", "accent"},
        {"selection", "selection"},             {"muted", "muted"},
        {"red", "danger"},
    };
    return map;
}

}

Theme::Theme(const QString &file, QObject *parent)
    : QObject(parent)
    , m_file(file)
    , m_colors(fallback())
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(100);
    connect(&m_debounce, &QTimer::timeout, this, [this] {
        reload();
        rewatch();
    });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    reload();
    rewatch();
}

QString Theme::defaultPath()
{
    return Paths::omarchyThemeFile();
}

QHash<QString, QString> Theme::parse(const QByteArray &text)
{
    QHash<QString, QString> out;
    for (const QByteArray &raw : text.split('\n')) {
        const QByteArray line = raw.trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith('['))
            continue;
        const int eq = line.indexOf('=');
        if (eq < 0)
            continue;
        const QString key = QString::fromUtf8(line.left(eq).trimmed());
        QString value = QString::fromUtf8(line.mid(eq + 1).trimmed());
        if (value.size() >= 2 && value.startsWith('"') && value.endsWith('"'))
            value = value.mid(1, value.size() - 2);
        out.insert(key, value);
    }
    return out;
}

void Theme::reload()
{
    QHash<QString, QColor> next = fallback();
    QHash<QString, QColor> palette;
    double fontBase = 12;

    QFile f(m_file);
    if (f.open(QIODevice::ReadOnly)) {
        const auto kv = parse(f.readAll());
        for (auto it = keyMap().cbegin(); it != keyMap().cend(); ++it) {
            const QColor c(kv.value(it.key()));
            if (c.isValid())
                next[it.value()] = c;
        }
        for (const QString &name : accentNames()) {
            const QColor c(kv.value(name));
            if (c.isValid())
                palette[name] = c;
        }
    }
    QFile shell(shellFileFor(m_file));
    if (shell.open(QIODevice::ReadOnly)) {
        const double base = parse(shell.readAll()).value("base-size").toDouble();
        if (base >= 6 && base <= 48)
            fontBase = base;
    }

    if (next != m_colors || palette != m_palette || fontBase != m_fontBase) {
        m_colors = next;
        m_palette = palette;
        m_fontBase = fontBase;
        emit changed();
    }
}

// Omarchy rewrites the theme directory on a switch, so watch the file, its
// directory and the parent, and re-add whatever still exists after each change.
void Theme::rewatch()
{
    const QFileInfo info(m_file);
    const QString dir = info.absolutePath();
    const QStringList wanted{m_file, shellFileFor(m_file), dir, QFileInfo(dir).absolutePath()};
    QStringList existing;
    for (const QString &p : wanted) {
        if (QFileInfo::exists(p))
            existing << p;
    }
    m_watcher.addPaths(existing);
}

QColor Theme::accent() const
{
    if (m_accentChoice != QLatin1String("theme")) {
        const QColor c = m_palette.value(m_accentChoice);
        if (c.isValid())
            return c;
    }
    return m_colors.value("accent");
}

QStringList Theme::accentChoices() const
{
    QStringList out{QStringLiteral("theme")};
    for (const QString &name : accentNames()) {
        if (m_palette.contains(name))
            out << name;
    }
    return out;
}

QColor Theme::paletteColor(const QString &name) const
{
    if (name == QLatin1String("theme"))
        return m_colors.value("accent");
    return m_palette.value(name);
}

void Theme::setAccentChoice(const QString &name)
{
    const QString next = name.isEmpty() ? QStringLiteral("theme") : name;
    if (next == m_accentChoice)
        return;
    m_accentChoice = next;
    emit changed();
}
