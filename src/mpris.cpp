#include "mpris.h"

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QUrl>

#include "library.h"
#include "player.h"

namespace {
const char kPath[] = "/org/mpris/MediaPlayer2";

QString loopStatus(const QString &repeat)
{
    return repeat == QLatin1String("one") ? QStringLiteral("Track")
         : repeat == QLatin1String("all") ? QStringLiteral("Playlist") : QStringLiteral("None");
}
}

class RootAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit)
    Q_PROPERTY(bool CanRaise READ canRaise)
    Q_PROPERTY(bool HasTrackList READ hasTrackList)
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry)
    Q_PROPERTY(QStringList SupportedUriSchemes READ none)
    Q_PROPERTY(QStringList SupportedMimeTypes READ none)
public:
    explicit RootAdaptor(Mpris *m) : QDBusAbstractAdaptor(m), m_mpris(m) {}
    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return QStringLiteral("OMP"); }
    QString desktopEntry() const { return QStringLiteral("omp"); }
    QStringList none() const { return {}; }
public slots:
    void Raise() { emit m_mpris->raiseRequested(); }
    void Quit() { emit m_mpris->quitRequested(); }
private:
    Mpris *m_mpris;
};

class PlayerAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(QString LoopStatus READ loopStatus WRITE setLoopStatus)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(bool Shuffle READ shuffle WRITE setShuffle)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double MinimumRate READ rate)
    Q_PROPERTY(double MaximumRate READ rate)
    Q_PROPERTY(bool CanGoNext READ yes)
    Q_PROPERTY(bool CanGoPrevious READ yes)
    Q_PROPERTY(bool CanPlay READ yes)
    Q_PROPERTY(bool CanPause READ yes)
    Q_PROPERTY(bool CanSeek READ yes)
    Q_PROPERTY(bool CanControl READ yes)
public:
    explicit PlayerAdaptor(Mpris *m) : QDBusAbstractAdaptor(m), m_mpris(m) {}
    QString playbackStatus() const { return m_mpris->playbackStatus(); }
    QString loopStatus() const { return ::loopStatus(m_mpris->player().repeat()); }
    void setLoopStatus(const QString &s)
    {
        m_mpris->player().setRepeat(s == QLatin1String("Track") ? "one" : s == QLatin1String("Playlist") ? "all" : "off");
    }
    double rate() const { return 1.0; }
    void setRate(double) {}
    bool shuffle() const { return m_mpris->player().shuffle(); }
    void setShuffle(bool on) { m_mpris->player().setShuffle(on); }
    QVariantMap metadata() const { return m_mpris->metadata(); }
    // mpv's 100 is full scale; it goes up to 130.
    double volume() const { return m_mpris->player().volume() / 100.0; }
    void setVolume(double v) { m_mpris->player().setVolume(qRound(qMax(0.0, v) * 100)); }
    qlonglong position() const { return qlonglong(m_mpris->player().position() * 1e6); }
    bool yes() const { return true; }
public slots:
    void Next() { m_mpris->player().next(); }
    void Previous() { m_mpris->player().previous(); }
    void Pause() { if (m_mpris->player().playing()) m_mpris->player().togglePause(); }
    void PlayPause() { m_mpris->player().togglePause(); }
    void Stop() { Pause(); }
    void Play() { if (!m_mpris->player().playing()) m_mpris->player().togglePause(); }
    void Seek(qlonglong offset) { m_mpris->player().seekBy(offset / 1e6); }
    void SetPosition(const QDBusObjectPath &track, qlonglong position)
    {
        if (track.path() == m_mpris->metadata().value("mpris:trackid").value<QDBusObjectPath>().path())
            m_mpris->player().seekTo(position / 1e6);
    }
    void OpenUri(const QString &) {}
private:
    Mpris *m_mpris;
};

Mpris::Mpris(Player &player, Library &library, const QString &service, QObject *parent)
    : QObject(parent), m_player(player), m_library(library)
{
    new RootAdaptor(this);
    new PlayerAdaptor(this);
    QDBusConnection bus = QDBusConnection::sessionBus();
    m_registered = bus.registerService(service) && bus.registerObject(kPath, this);

    connect(&m_player, &Player::playingChanged, this, [this] { emitChanged({"PlaybackStatus"}); });
    connect(&m_player, &Player::currentChanged, this, [this] { emitChanged({"Metadata", "PlaybackStatus"}); });
    connect(&m_player, &Player::durationChanged, this, [this] { emitChanged({"Metadata"}); });
    connect(&m_library, &Library::coversChanged, this, [this] { emitChanged({"Metadata"}); });
    connect(&m_player, &Player::volumeChanged, this, [this] { emitChanged({"Volume"}); });
    connect(&m_player, &Player::shuffleChanged, this, [this] { emitChanged({"Shuffle"}); });
    connect(&m_player, &Player::repeatChanged, this, [this] { emitChanged({"LoopStatus"}); });
}

QString Mpris::playbackStatus() const
{
    if (m_player.current().isEmpty())
        return QStringLiteral("Stopped");
    return m_player.playing() ? QStringLiteral("Playing") : QStringLiteral("Paused");
}

QVariantMap Mpris::metadata()
{
    const QVariantMap cur = m_player.current();
    QVariantMap m;
    if (cur.isEmpty()) {
        m.insert("mpris:trackid", QVariant::fromValue(QDBusObjectPath("/org/mpris/MediaPlayer2/TrackList/NoTrack")));
        return m;
    }
    m.insert("mpris:trackid", QVariant::fromValue(QDBusObjectPath(
        QStringLiteral("/org/omp/track/%1").arg(m_player.currentIndex()))));
    m.insert("mpris:length", qlonglong(m_player.duration() * 1e6));
    m.insert("xesam:title", cur.value("title").toString());
    m.insert("xesam:artist", QStringList{cur.value("artist").toString()});
    m.insert("xesam:album", cur.value("album").toString());
    m.insert("xesam:url", QUrl::fromLocalFile(cur.value("path").toString()).toString());
    const QString art = m_library.coverFor(cur.value("path").toString());
    if (!art.isEmpty())
        m.insert("mpris:artUrl", art);
    return m;
}

void Mpris::emitChanged(const QStringList &names)
{
    if (!m_registered)
        return;
    QVariantMap changed;
    for (const QString &n : names) {
        if (n == "PlaybackStatus") changed.insert(n, playbackStatus());
        else if (n == "Metadata") changed.insert(n, metadata());
        else if (n == "Volume") changed.insert(n, m_player.volume() / 100.0);
        else if (n == "Shuffle") changed.insert(n, m_player.shuffle());
        else if (n == "LoopStatus") changed.insert(n, loopStatus(m_player.repeat()));
    }
    QDBusMessage sig = QDBusMessage::createSignal(kPath, "org.freedesktop.DBus.Properties", "PropertiesChanged");
    sig << QStringLiteral("org.mpris.MediaPlayer2.Player") << changed << QStringList();
    QDBusConnection::sessionBus().send(sig);
}

#include "mpris.moc"
