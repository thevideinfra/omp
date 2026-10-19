#pragma once

#include <QObject>
#include <QString>

// "Start at login": an XDG autostart entry, which the session (uwsm) runs at
// login. The state is whether the file exists.
class Autostart : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)

public:
    explicit Autostart(const QString &dir = defaultDir(),
                       const QString &exec = QString(), QObject *parent = nullptr);

    static QString defaultDir();

    bool enabled() const;
    void setEnabled(bool on);

signals:
    void changed();

private:
    QString m_file;
    QString m_exec;
};
