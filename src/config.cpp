#include "config.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include "paths.h"

namespace Config {

bool startsArray(const QString &value)
{
    return value.trimmed().startsWith(QLatin1Char('['));
}

bool closesArray(const QString &text)
{
    bool inQuote = false;
    for (const QChar c : text) {
        if (c == QLatin1Char('"'))
            inQuote = !inQuote;
        else if (!inQuote && c == QLatin1Char('#'))
            return false;
        else if (!inQuote && c == QLatin1Char(']'))
            return true;
    }
    return false;
}

QStringList parseFolders(const QByteArray &text)
{
    static const QRegularExpression quoted(QStringLiteral("\"([^\"]*)\""));
    QStringList out;
    bool inArray = false;
    for (const QByteArray &raw : text.split('\n')) {
        QString line = QString::fromUtf8(raw).trimmed();
        if (inArray && line.startsWith(QLatin1Char('#')))
            continue;
        if (!inArray) {
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq < 0 || line.left(eq).trimmed() != QStringLiteral("folders"))
                continue;
            line = line.mid(eq + 1);
            if (!startsArray(line))
                continue;   // folders = "x" is not a list
            inArray = true;
        }
        const bool closed = closesArray(line);
        auto it = quoted.globalMatch(line);
        while (it.hasNext()) {
            QString path = it.next().captured(1);
            if (path == QStringLiteral("~") || path.startsWith(QStringLiteral("~/")))
                path = QDir::homePath() + path.mid(1);
            out << QDir::cleanPath(path);
        }
        if (closed)
            inArray = false;
    }
    if (out.isEmpty())
        out << QDir::homePath() + QStringLiteral("/Music");
    return out;
}

QStringList folders()
{
    QFile f(Paths::configDir() + QStringLiteral("/config.toml"));
    return parseFolders(f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray());
}

}
