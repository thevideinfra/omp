#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QTimer>

class Theme : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QColor background READ background NOTIFY changed)
    Q_PROPERTY(QColor surface READ surface NOTIFY changed)
    Q_PROPERTY(QColor sunken READ sunken NOTIFY changed)
    Q_PROPERTY(QColor foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QColor dimForeground READ dimForeground NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    Q_PROPERTY(QColor selection READ selection NOTIFY changed)
    Q_PROPERTY(QColor muted READ muted NOTIFY changed)
    Q_PROPERTY(QColor danger READ danger NOTIFY changed)

public:
    explicit Theme(const QString &file = defaultPath(), QObject *parent = nullptr);

    static QString defaultPath();
    static QHash<QString, QString> parse(const QByteArray &text);

    QColor background() const { return m_colors.value("background"); }
    QColor surface() const { return m_colors.value("surface"); }
    QColor sunken() const { return m_colors.value("sunken"); }
    QColor foreground() const { return m_colors.value("foreground"); }
    QColor dimForeground() const { return m_colors.value("dimForeground"); }
    QColor accent() const { return m_colors.value("accent"); }
    QColor selection() const { return m_colors.value("selection"); }
    QColor muted() const { return m_colors.value("muted"); }
    QColor danger() const { return m_colors.value("danger"); }

signals:
    void changed();

private:
    void reload();
    void rewatch();

    QString m_file;
    QHash<QString, QColor> m_colors;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
