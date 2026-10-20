#include "player.h"

#include <QFile>
#include <QRandomGenerator>

#include <algorithm>

#include <csignal>
#include <sys/prctl.h>

#include "equalizer.h"
#include "paths.h"
#include "track.h"

Player::Player(const QString &mpvProgram, const QStringList &extraArgs, const QString &socketPath,
               QObject *parent)
    : QObject(parent)
    , m_program(mpvProgram)
    , m_extraArgs(extraArgs)
    , m_socketPath(socketPath.isEmpty() ? Paths::runtimeDir() + QStringLiteral("/mpv.sock")
                                        : socketPath)
    , m_queue(Track::roleNames())
{
    connect(&m_client, &MpvClient::connected, this, &Player::onConnected);
    connect(&m_client, &MpvClient::disconnected, this, [this] { setReady(false); });
    connect(&m_client, &MpvClient::propertyChanged, this, &Player::onProperty);
    connect(&m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            m_retry.stop();
            setError(tr("mpv not found or failed to start"));
        }
    });
    connect(&m_proc, &QProcess::finished, this, &Player::onProcessFinished);

    m_sleepTick.setInterval(1000);
    connect(&m_sleepTick, &QTimer::timeout, this, [this] {
        if (--m_sleepRemaining > 0) {
            emit sleepChanged();
            return;
        }
        m_sleepTick.stop();
        m_sleepRemaining = -1;
        m_client.setProperty("pause", true);   // time is up
        emit sleepChanged();
    });

    m_retry.setInterval(50);
    connect(&m_retry, &QTimer::timeout, this, [this] {
        if (++m_attempts > 100) {
            m_retry.stop();
            setError(tr("mpv socket never appeared"));
            return;
        }
        m_client.connectTo(m_socketPath);
    });
}

Player::~Player()
{
    m_shuttingDown = true;
    if (m_proc.state() != QProcess::NotRunning) {
        m_proc.terminate();
        if (!m_proc.waitForFinished(1000)) {
            m_proc.kill();
            m_proc.waitForFinished(1000);
        }
    }
}

namespace {
// mpv calls "off" no.
QString mpvReplayGain(const QString &mode)
{
    return mode == QLatin1String("off") ? QStringLiteral("no") : mode;
}
}

void Player::start()
{
    QFile::remove(m_socketPath);   // stale socket from a crashed run
    m_attempts = 0;
    QStringList args{"--idle=yes",
                     "--no-video",
                     "--no-terminal",
                     "--load-scripts=no",   // OMP has its own MPRIS service
                     "--audio-display=no",
                     QStringLiteral("--gapless-audio=") + (m_gapless ? "yes" : "no"),
                     "--prefetch-playlist=yes",
                     QStringLiteral("--replaygain=") + mpvReplayGain(m_replayGain),
                     QStringLiteral("--replaygain-preamp=") + QString::number(m_preampDb),
                     "--replaygain-clip=no",   // lower the gain rather than clip
                     "--volume=" + QString::number(m_startVolume),
                     QStringLiteral("--loop-playlist=") + (m_repeat == QLatin1String("all") ? "inf" : "no"),
                     QStringLiteral("--loop-file=") + (m_repeat == QLatin1String("one") ? "inf" : "no"),
                     "--input-ipc-server=" + m_socketPath};
    if (const QString af = Equalizer::filter(m_eqPreset); !af.isEmpty())
        args << QStringLiteral("--af=") + af;
    args += m_extraArgs;
    // Have the kernel stop mpv if we die without running destructors.
    m_proc.setChildProcessModifier([] { ::prctl(PR_SET_PDEATHSIG, SIGTERM); });
    m_proc.start(m_program, args);
    m_retry.start();
}

void Player::onConnected()
{
    m_retry.stop();
    for (const char *property : {"pause", "idle-active", "time-pos", "duration", "volume", "playlist-pos", "eof-reached"})
        m_client.observe(QString::fromLatin1(property));
    setError(QString());
    setReady(true);
}

void Player::onProcessFinished()
{
    setReady(false);
    m_retry.stop();
    m_client.connectTo(QString());   // drop the dead socket
    if (m_shuttingDown)
        return;
    if (m_restarts++ < 1) {
        m_queue.setRows({});
        QTimer::singleShot(500, this, &Player::start);
    } else {
        setError(tr("mpv exited"));
    }
}

void Player::onProperty(const QString &name, const QVariant &value)
{
    if (name == QLatin1String("pause")) {
        m_paused = value.toBool();
        updatePlaying();
    } else if (name == QLatin1String("idle-active")) {
        m_idle = value.toBool();
        updatePlaying();
    } else if (name == QLatin1String("time-pos")) {
        const double v = value.isValid() ? value.toDouble() : 0.0;
        if (!qFuzzyCompare(1 + m_position, 1 + v)) {
            m_position = v;
            emit positionChanged();
        }
    } else if (name == QLatin1String("duration")) {
        const double v = value.isValid() ? value.toDouble() : 0.0;
        if (!qFuzzyCompare(1 + m_duration, 1 + v)) {
            m_duration = v;
            emit durationChanged();
        }
    } else if (name == QLatin1String("volume")) {
        const int v = qRound(value.toDouble());
        if (v != m_volume) {
            m_volume = v;
            m_startVolume = qBound(0, v, 130);   // a restarted mpv keeps it
            emit volumeChanged();
        }
    } else if (name == QLatin1String("eof-reached")) {
        if (value.toBool() && m_sleepAfterSong) {
            // keep-open held mpv at the end of the song: let the next one load, but wait there.
            m_sleepAfterSong = false;
            m_client.setProperty("keep-open", "no");
            m_client.command({QStringLiteral("playlist-next")});
            m_client.setProperty("pause", true);
            emit sleepChanged();
        }
    } else if (name == QLatin1String("playlist-pos")) {
        const int i = value.isValid() ? value.toInt() : -1;
        if (i != m_index) {
            m_index = i;
            emit currentIndexChanged();
        }
        emit currentChanged();
    }
}

void Player::updatePlaying()
{
    const bool playing = !m_paused && !m_idle;
    if (playing != m_playing) {
        m_playing = playing;
        emit playingChanged();
    }
}

void Player::setReady(bool ready)
{
    if (ready != m_ready) {
        m_ready = ready;
        emit readyChanged();
    }
}

void Player::setError(const QString &error)
{
    if (error != m_error) {
        m_error = error;
        emit errorChanged();
    }
}

QVariantMap Player::current() const
{
    if (m_index >= 0 && m_index < m_queue.rowCount())
        return m_queue.rows().at(m_index);
    return {};
}

void Player::setVolume(int volume)
{
    m_client.setProperty("volume", qBound(0, volume, 130));
}

void Player::playTracks(const QVariantList &tracks, int startIndex)
{
    if (tracks.isEmpty())
        return;
    QVector<QVariantMap> rows;
    for (int i = 0; i < tracks.size(); ++i) {
        QVariantMap row = tracks.at(i).toMap();
        row.insert(QStringLiteral("seq"), i);   // the unshuffled order
        rows.append(row);
    }
    if (m_shuffle) {
        // The chosen song first, every other song after it in random order.
        const QVariantMap first = rows.at(qBound(0, startIndex, int(rows.size()) - 1));
        rows.remove(qBound(0, startIndex, int(rows.size()) - 1));
        std::shuffle(rows.begin(), rows.end(), *QRandomGenerator::global());
        rows.prepend(first);
        startIndex = 0;
    }
    m_queue.setRows(rows);
    for (int i = 0; i < rows.size(); ++i) {
        m_client.command({QStringLiteral("loadfile"), resolve(rows.at(i).value("path").toString()),
                          i == 0 ? QStringLiteral("replace") : QStringLiteral("append")});
    }
    if (startIndex > 0)
        m_client.setProperty("playlist-pos", startIndex);
    m_client.setProperty("pause", false);
    emit currentChanged();
}

void Player::enqueue(const QVariantMap &track)
{
    int seq = 0;
    for (const QVariantMap &r : m_queue.rows())
        seq = qMax(seq, r.value(QStringLiteral("seq"), -1).toInt() + 1);
    QVariantMap row = track;
    row.insert(QStringLiteral("seq"), qMax(seq, int(m_queue.rowCount())));
    m_queue.append(row);
    m_client.command({QStringLiteral("loadfile"), resolve(track.value("path").toString()),
                      QStringLiteral("append-play")});
    emit currentChanged();
}

void Player::playNext(const QVariantList &tracks)
{
    if (tracks.isEmpty())
        return;
    if (m_idle || m_index < 0 || m_index >= m_queue.rowCount()) {
        playTracks(tracks, 0);
        return;
    }
    QVector<QVariantMap> rows = m_queue.rows();
    int seq = 0;
    for (const QVariantMap &r : std::as_const(rows))
        seq = qMax(seq, r.value(QStringLiteral("seq"), -1).toInt() + 1);
    for (int i = 0; i < tracks.size(); ++i) {
        QVariantMap row = tracks.at(i).toMap();
        row.insert(QStringLiteral("seq"), seq + i);
        rows.insert(m_index + 1 + i, row);
    }
    m_queue.setRows(rows);
    // insert-next puts each song straight after the playing one, so go backwards to keep the order.
    for (int i = tracks.size() - 1; i >= 0; --i) {
        m_client.command({QStringLiteral("loadfile"), resolve(tracks.at(i).toMap().value("path").toString()),
                          QStringLiteral("insert-next")});
    }
    emit currentChanged();
}

void Player::enqueueTracks(const QVariantList &tracks)
{
    for (const QVariant &t : tracks)
        enqueue(t.toMap());
}

void Player::togglePause()
{
    m_client.command({QStringLiteral("cycle"), QStringLiteral("pause")});
}

void Player::next()
{
    m_client.command({QStringLiteral("playlist-next")});
}

void Player::previous()
{
    m_client.command({QStringLiteral("playlist-prev")});
}

void Player::seekBy(double seconds)
{
    m_client.command({QStringLiteral("seek"), seconds, QStringLiteral("relative")});
}

void Player::seekTo(double seconds)
{
    m_client.command({QStringLiteral("seek"), seconds, QStringLiteral("absolute")});
}

void Player::jumpTo(int index)
{
    m_client.setProperty("playlist-pos", index);
    m_client.setProperty("pause", false);
}

void Player::startSleepSeconds(int seconds)
{
    if (seconds <= 0)
        return;
    cancelSleep();
    m_sleepRemaining = seconds;
    m_sleepTick.start();
    emit sleepChanged();
}

void Player::sleepAfterCurrentSong()
{
    cancelSleep();
    m_sleepAfterSong = true;
    m_client.setProperty("keep-open", "always");   // pause at the end of each song instead of moving on
    emit sleepChanged();
}

void Player::cancelSleep()
{
    const bool was = m_sleepRemaining >= 0 || m_sleepAfterSong;
    m_sleepTick.stop();
    m_sleepRemaining = -1;
    if (m_sleepAfterSong)
        m_client.setProperty("keep-open", "no");
    m_sleepAfterSong = false;
    if (was)
        emit sleepChanged();
}

void Player::removeAt(int index)
{
    if (index < 0 || index >= m_queue.rowCount())
        return;
    QVector<QVariantMap> rows = m_queue.rows();
    rows.remove(index);
    m_queue.setRows(rows);
    m_client.command({QStringLiteral("playlist-remove"), index});
    if (index < m_index) {   // an earlier song went: the playing one moved up a place
        --m_index;
        emit currentIndexChanged();
    }
    emit currentChanged();
}

void Player::moveRow(int from, int to)
{
    const int n = m_queue.rowCount();
    if (from < 0 || from >= n || to < 0 || to >= n || from == to)
        return;
    QVector<QVariantMap> rows = m_queue.rows();
    rows.insert(to, rows.takeAt(from));
    m_queue.setRows(rows);
    // mpv's playlist-move puts the entry *before* the given index.
    m_client.command({QStringLiteral("playlist-move"), from, to > from ? to + 1 : to});
    if (m_index == from)
        m_index = to;
    else if (from < m_index && to >= m_index)
        --m_index;
    else if (from > m_index && to <= m_index)
        ++m_index;
    emit currentIndexChanged();
    emit currentChanged();
}

void Player::clearQueue()
{
    // mpv's playlist-clear keeps the entry that is playing, so the song carries on.
    m_client.command({QStringLiteral("playlist-clear")});
    QVector<QVariantMap> keep;
    if (!m_idle && m_index >= 0 && m_index < m_queue.rowCount())
        keep.append(m_queue.rows().at(m_index));
    m_queue.setRows(keep);
    emit currentChanged();
}

void Player::setShuffle(bool on)
{
    if (on == m_shuffle)
        return;
    m_shuffle = on;
    if (on) {
        // Remember the order so that switching shuffle off can restore it.
        QVector<QVariantMap> rows = m_queue.rows();
        for (int i = 0; i < rows.size(); ++i)
            rows[i].insert(QStringLiteral("seq"), i);
        m_queue.setRows(rows);
        reorderUpcoming([](QVector<QVariantMap> &upcoming) {
            std::shuffle(upcoming.begin(), upcoming.end(), *QRandomGenerator::global());
        });
    } else {
        reorderUpcoming([](QVector<QVariantMap> &upcoming) {
            std::stable_sort(upcoming.begin(), upcoming.end(), [](const QVariantMap &a, const QVariantMap &b) {
                return a.value(QStringLiteral("seq"), 1 << 30).toInt() < b.value(QStringLiteral("seq"), 1 << 30).toInt();
            });
        });
    }
    emit shuffleChanged();
}

void Player::setRepeat(const QString &mode)
{
    if (mode == m_repeat || !QStringList{"off", "all", "one"}.contains(mode))
        return;
    m_repeat = mode;
    applyRepeat();
    emit repeatChanged();
}

void Player::cycleRepeat()
{
    setRepeat(m_repeat == QLatin1String("off") ? QStringLiteral("all")
              : m_repeat == QLatin1String("all") ? QStringLiteral("one") : QStringLiteral("off"));
}

void Player::setPlaybackOptions(const QString &replayGain, int preampDb, bool gapless)
{
    if (replayGain == m_replayGain && preampDb == m_preampDb && gapless == m_gapless)
        return;
    m_replayGain = replayGain;
    m_preampDb = preampDb;
    m_gapless = gapless;
    if (!m_client.isConnected())
        return;   // mpv starts with them
    m_client.setProperty("replaygain", mpvReplayGain(replayGain));
    m_client.setProperty("replaygain-preamp", preampDb);
    m_client.setProperty("gapless-audio", gapless ? "yes" : "no");
}

void Player::applyRepeat()
{
    m_client.setProperty("loop-playlist", m_repeat == QLatin1String("all") ? "inf" : "no");
    m_client.setProperty("loop-file", m_repeat == QLatin1String("one") ? "inf" : "no");
}

void Player::setEqPreset(const QString &preset)
{
    if (preset == m_eqPreset || !Equalizer::presetNames().contains(preset))
        return;
    m_eqPreset = preset;
    applyEqualizer();
    emit eqPresetChanged();
}

void Player::applyEqualizer()
{
    m_client.command({QStringLiteral("af"), QStringLiteral("set"), Equalizer::filter(m_eqPreset)});
}

QStringList Player::eqPresets() const
{
    return Equalizer::presetNames();
}

QVariantList Player::eqGains(const QString &preset) const
{
    return Equalizer::gains(preset);
}

void Player::setStartPlayback(bool shuffle, const QString &repeat, const QString &eqPreset)
{
    m_shuffle = shuffle;
    if (QStringList{"off", "all", "one"}.contains(repeat))
        m_repeat = repeat;
    if (Equalizer::presetNames().contains(eqPreset))
        m_eqPreset = eqPreset;
}

// Reorders the songs after the playing one (those before it, and it, stay put):
// they are taken out of mpv's playlist and appended again in the new order.
void Player::reorderUpcoming(const std::function<void(QVector<QVariantMap> &)> &order)
{
    const int n = m_queue.rowCount();
    if (m_index < 0 || m_index >= n - 1)
        return;
    QVector<QVariantMap> rows = m_queue.rows();
    QVector<QVariantMap> upcoming = rows.mid(m_index + 1);
    order(upcoming);
    for (int i = 0; i < upcoming.size(); ++i)
        m_client.command({QStringLiteral("playlist-remove"), m_index + 1});
    for (const QVariantMap &r : std::as_const(upcoming))
        m_client.command({QStringLiteral("loadfile"), resolve(r.value(QStringLiteral("path")).toString()),
                          QStringLiteral("append")});
    rows.resize(m_index + 1);
    rows += upcoming;
    m_queue.setRows(rows);
    emit currentChanged();
}
