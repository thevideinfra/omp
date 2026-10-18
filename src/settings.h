#pragma once

#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantMap>

class Settings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString density READ density NOTIFY changed)
    Q_PROPERTY(QString fontSize READ fontSize NOTIFY changed)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY changed)
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(bool panelCover READ panelCover NOTIFY changed)
    Q_PROPERTY(bool panelSeek READ panelSeek NOTIFY changed)
    Q_PROPERTY(bool panelVolume READ panelVolume NOTIFY changed)
    Q_PROPERTY(bool panelUpNext READ panelUpNext NOTIFY changed)
    Q_PROPERTY(int panelUpNextCount READ panelUpNextCount NOTIFY changed)
    Q_PROPERTY(double densityScale READ densityScale NOTIFY changed)
    Q_PROPERTY(double fontScale READ fontScale NOTIFY changed)

public:
    explicit Settings(const QString &file = defaultPath(), QObject *parent = nullptr);
    ~Settings() override;

    static QString defaultPath();
    static QVariantMap parse(const QByteArray &text);
    static QByteArray apply(const QByteArray &existing, const QVariantMap &updates);
    static double densityScaleFor(const QString &density);
    static double fontScaleFor(const QString &size);

    QString density() const { return m_values.value("density").toString(); }
    QString fontSize() const { return m_values.value("fontSize").toString(); }
    QString fontFamily() const { return m_values.value("fontFamily").toString(); }
    QString accent() const { return m_values.value("accent").toString(); }
    bool panelCover() const { return m_values.value("panelCover").toBool(); }
    bool panelSeek() const { return m_values.value("panelSeek").toBool(); }
    bool panelVolume() const { return m_values.value("panelVolume").toBool(); }
    bool panelUpNext() const { return m_values.value("panelUpNext").toBool(); }
    int panelUpNextCount() const { return m_values.value("panelUpNextCount").toInt(); }
    double densityScale() const { return densityScaleFor(density()); }
    double fontScale() const { return fontScaleFor(fontSize()); }

    Q_INVOKABLE void set(const QString &prop, const QVariant &value);
    Q_INVOKABLE QStringList fontFamilies() const;
    void save();

signals:
    void changed();

private:
    QString m_file;
    QVariantMap m_values;
    QSet<QString> m_dirty;
    QTimer m_saveTimer;
};
