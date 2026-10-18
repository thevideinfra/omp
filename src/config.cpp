#include "config.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include "paths.h"

namespace Config {

QStringList parseFolders(const QByteArray &text)
{
    static const QRegularExpression quoted(QStringLiteral("\"([^\"]*)\""));
    QStringList out;
    for (const QByteArray &raw : text.split('\n')) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (!line.startsWith(QStringLiteral("folders")))
            continue;
        auto it = quoted.globalMatch(line);
        while (it.hasNext()) {
            QString path = it.next().captured(1);
            if (path == QStringLiteral("~") || path.startsWith(QStringLiteral("~/")))
                path = QDir::homePath() + path.mid(1);
            out << QDir::cleanPath(path);
        }
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
