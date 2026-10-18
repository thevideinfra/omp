#pragma once

#include <QObject>
#include <QStringList>
#include <QVariant>

class Settings;

// One scope (the app's or the tray panel's) of Settings, as QML sees it under
// the name `ui`. The look properties read the app keys or the panel* keys; the
// panel's section options are forwarded from the same Settings in both scopes.
class LookScope : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString density READ density NOTIFY changed)
    Q_PROPERTY(QString fontSize READ fontSize NOTIFY changed)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY changed)
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(double densityScale READ densityScale NOTIFY changed)
    Q_PROPERTY(double fontScale READ fontScale NOTIFY changed)
    Q_PROPERTY(bool panelCover READ panelCover NOTIFY changed)
    Q_PROPERTY(bool panelSeek READ panelSeek NOTIFY changed)
    Q_PROPERTY(bool panelVolume READ panelVolume NOTIFY changed)
    Q_PROPERTY(bool panelUpNext READ panelUpNext NOTIFY changed)
    Q_PROPERTY(int panelUpNextCount READ panelUpNextCount NOTIFY changed)

public:
    LookScope(Settings *settings, bool panel, QObject *parent = nullptr);

    QString density() const;
    QString fontSize() const;
    QString fontFamily() const;
    QString accent() const;
    double densityScale() const;
    double fontScale() const;
    bool panelCover() const;
    bool panelSeek() const;
    bool panelVolume() const;
    bool panelUpNext() const;
    int panelUpNextCount() const;

    Q_INVOKABLE void set(const QString &prop, const QVariant &value);
    Q_INVOKABLE QStringList fontFamilies() const;

signals:
    void changed();

private:
    Settings *m_settings;
    bool m_panel;
};
