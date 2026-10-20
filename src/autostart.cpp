#include "autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QDebug>

Autostart::Autostart(const QString &dir, const QString &exec, QObject *parent)
    : QObject(parent)
    , m_file(dir + QStringLiteral("/omp.desktop"))
    , m_exec(exec.isEmpty() ? QCoreApplication::applicationFilePath() : exec)
{
}

QString Autostart::defaultDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QStringLiteral("/autostart");
}

bool Autostart::enabled() const
{
    return QFileInfo::exists(m_file);
}

void Autostart::setEnabled(bool on)
{
    if (on == enabled())
        return;
    if (on) {
        if (!QDir().mkpath(QFileInfo(m_file).absolutePath())) {
            qWarning() << "cannot create" << QFileInfo(m_file).absolutePath();
            return;
        }
        QString exec = m_exec;
        exec.replace('\\', QStringLiteral("\\\\")).replace('"', QStringLiteral("\\\""));
        QSaveFile f(m_file);
        if (!f.open(QIODevice::WriteOnly)) {
            qWarning() << "cannot write" << m_file << f.errorString();
            return;
        }
        f.write("[Desktop Entry]\nType=Application\nName=OMP\nComment=Music player\n");
        f.write(QStringLiteral("Exec=\"%1\" tray\n").arg(exec).toUtf8());
        f.write("Terminal=false\nX-GNOME-Autostart-enabled=true\n");
        if (!f.commit()) {
            qWarning() << "cannot write" << m_file;
            return;
        }
    } else if (!QFile::remove(m_file)) {
        qWarning() << "cannot remove" << m_file;
        return;
    }
    emit changed();
}
