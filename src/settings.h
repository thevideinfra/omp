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
    Q_PROPERTY(QStringList folders READ folders NOTIFY changed)
    Q_PROPERTY(bool showAccent READ showAccent NOTIFY changed)
    Q_PROPERTY(bool saveLayout READ saveLayout NOTIFY changed)
    Q_PROPERTY(bool saveSort READ saveSort NOTIFY changed)
    Q_PROPERTY(QStringList songColumns READ songColumns NOTIFY changed)
    Q_PROPERTY(QStringList hiddenTabs READ hiddenTabs NOTIFY changed)
    Q_PROPERTY(bool artistAlbumsColumn READ artistAlbumsColumn NOTIFY changed)
    Q_PROPERTY(QString replayGain READ replayGain NOTIFY changed)
    Q_PROPERTY(int replayGainPreamp READ replayGainPreamp NOTIFY changed)
    Q_PROPERTY(bool gapless READ gapless NOTIFY changed)
    Q_PROPERTY(bool hideServerDuplicates READ hideServerDuplicates NOTIFY changed)
    Q_PROPERTY(bool railCollapsed READ railCollapsed NOTIFY changed)
    Q_PROPERTY(bool showSourceTags READ showSourceTags NOTIFY changed)
    Q_PROPERTY(bool serverEnabled READ serverEnabled NOTIFY changed)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY changed)
    Q_PROPERTY(QString serverUser READ serverUser NOTIFY changed)
    Q_PROPERTY(QString serverType READ serverType NOTIFY changed)
    Q_PROPERTY(bool panelShowAccent READ panelShowAccent NOTIFY changed)
    Q_PROPERTY(bool panelCover READ panelCover NOTIFY changed)
    Q_PROPERTY(bool panelSeek READ panelSeek NOTIFY changed)
    Q_PROPERTY(bool panelVolume READ panelVolume NOTIFY changed)
    Q_PROPERTY(bool panelUpNext READ panelUpNext NOTIFY changed)
    Q_PROPERTY(int panelUpNextCount READ panelUpNextCount NOTIFY changed)
    Q_PROPERTY(double densityScale READ densityScale NOTIFY changed)
    Q_PROPERTY(double fontScale READ fontScale NOTIFY changed)
    Q_PROPERTY(QString panelDensity READ panelDensity NOTIFY changed)
    Q_PROPERTY(QString panelFontSize READ panelFontSize NOTIFY changed)
    Q_PROPERTY(QString panelFontFamily READ panelFontFamily NOTIFY changed)
    Q_PROPERTY(QString panelAccent READ panelAccent NOTIFY changed)
    Q_PROPERTY(double panelDensityScale READ panelDensityScale NOTIFY changed)
    Q_PROPERTY(double panelFontScale READ panelFontScale NOTIFY changed)

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
    bool shuffle() const { return m_values.value("shuffle").toBool(); }
    QString repeat() const { return m_values.value("repeat").toString(); }
    QString eqPreset() const { return m_values.value("eqPreset").toString(); }
    QStringList folders() const { return m_values.value("folders").toStringList(); }
    int volume() const { return m_values.value("volume").toInt(); }
    bool serverEnabled() const { return m_values.value("serverEnabled").toBool(); }
    QString serverUrl() const { return m_values.value("serverUrl").toString(); }
    QString serverType() const { return m_values.value("serverType").toString(); }   // "navidrome", "airsonic"...
    QString serverUser() const { return m_values.value("serverUser").toString(); }
    bool railCollapsed() const { return m_values.value("railCollapsed").toBool(); }
    bool showSourceTags() const { return m_values.value("showSourceTags").toBool(); }
    // Artists tab: true = artists | albums | songs, false = artists | all of the artist's albums and songs.
    bool artistAlbumsColumn() const { return m_values.value("artistAlbumsColumn").toBool(); }
    QString replayGain() const { return m_values.value("replayGain").toString(); }          // "off", "track" or "album"
    int replayGainPreamp() const { return m_values.value("replayGainPreamp").toInt(); }      // dB, -15..15
    // Leave out a server song when the same one (artist, title, album, about the same length) is in a local folder.
    bool hideServerDuplicates() const { return m_values.value("hideServerDuplicates").toBool(); }
    bool gapless() const { return m_values.value("gapless").toBool(); }
    bool saveSort() const { return m_values.value("saveSort").toBool(); }
    QString sortSongs() const { return m_values.value("sortSongs").toString(); }
    QString sortAlbums() const { return m_values.value("sortAlbums").toString(); }
    QString sortArtists() const { return m_values.value("sortArtists").toString(); }
    bool saveLayout() const { return m_values.value("saveLayout").toBool(); }
    bool showAccent() const { return m_values.value("showAccent").toBool(); }
    bool panelShowAccent() const { return m_values.value("panelShowAccent").toBool(); }
    bool panelCover() const { return m_values.value("panelCover").toBool(); }
    bool panelSeek() const { return m_values.value("panelSeek").toBool(); }
    bool panelVolume() const { return m_values.value("panelVolume").toBool(); }
    bool panelUpNext() const { return m_values.value("panelUpNext").toBool(); }
    int panelUpNextCount() const { return m_values.value("panelUpNextCount").toInt(); }
    double densityScale() const { return densityScaleFor(density()); }
    double fontScale() const { return fontScaleFor(fontSize()); }
    QString panelDensity() const { return m_values.value("panelDensity").toString(); }
    QString panelFontSize() const { return m_values.value("panelFontSize").toString(); }
    QString panelFontFamily() const { return m_values.value("panelFontFamily").toString(); }
    QString panelAccent() const { return m_values.value("panelAccent").toString(); }
    double panelDensityScale() const { return densityScaleFor(panelDensity()); }
    double panelFontScale() const { return fontScaleFor(panelFontSize()); }

    // Music folders; the last one cannot be removed.
    Q_INVOKABLE void addFolder(const QString &path);
    Q_INVOKABLE void removeFolder(const QString &path);
    Q_INVOKABLE void set(const QString &prop, const QVariant &value);
    // Column sizes of a tab ("album", "artist" or "playlist") as fractions of its width,
    // "list,middle" or "list"; empty when none are stored.
    Q_INVOKABLE QString layoutFor(const QString &tab) const;
    Q_INVOKABLE void setLayoutFor(const QString &tab, const QString &fractions);
    // Tabs hidden from the left panel (Settings itself always shows; one other tab always stays).
    QStringList hiddenTabs() const;
    Q_INVOKABLE bool tabShown(const QString &name) const;
    Q_INVOKABLE void setTabShown(const QString &name, bool shown);
    // The columns of the song tables, in table order ("title" always among them).
    QStringList songColumns() const;
    Q_INVOKABLE void setSongColumn(const QString &column, bool shown);
    // Forgets the stored sizes and tells the open tabs to go back to equal columns.
    Q_INVOKABLE void resetLayout();
    Q_INVOKABLE QStringList fontFamilies() const;
    void save();

signals:
    void changed();
    void layoutReset();

private:
    QString m_file;
    QVariantMap m_values;
    QSet<QString> m_dirty;
    QTimer m_saveTimer;
};
