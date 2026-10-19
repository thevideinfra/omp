#include "player.h"

#include <QFile>

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

void Player::start()
{
    QFile::remove(m_socketPath);   // stale socket from a crashed run
    m_attempts = 0;
    QStringList args{"--idle=yes",
                     "--no-video",
                     "--no-terminal",
                     "--audio-display=no",
                     "--gapless-audio=yes",
                     "--prefetch-playlist=yes",
                     "--replaygain=album",
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
    for (const char *property : {"pause", "idle-active", "time-pos", "duration", "volume", "playlist-pos", "playlist"})
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
    } else if (name == QLatin1String("playlist")) {
        syncQueueOrder(value.toList());
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
    for (const QVariant &t : tracks)
        rows.append(t.toMap());
    m_queue.setRows(rows);
    for (int i = 0; i < rows.size(); ++i) {
        m_client.command({QStringLiteral("loadfile"), rows.at(i).value("path").toString(),
                          i == 0 ? QStringLiteral("replace") : QStringLiteral("append")});
    }
    if (startIndex > 0)
        m_client.setProperty("playlist-pos", startIndex);
    m_client.setProperty("pause", false);
    if (m_shuffle)
        m_client.command({QStringLiteral("playlist-shuffle")});   // the playing song stays
    emit currentChanged();
}

void Player::enqueue(const QVariantMap &track)
{
    m_queue.append(track);
    m_client.command({QStringLiteral("loadfile"), track.value("path").toString(),
                      QStringLiteral("append-play")});
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
    m_client.command({on ? QStringLiteral("playlist-shuffle") : QStringLiteral("playlist-unshuffle")});
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

// mpv shuffled or unshuffled its playlist: put the queue rows in the same order.
// Ignored while the two disagree in length (a load is still in flight).
void Player::syncQueueOrder(const QVariantList &playlist)
{
    if (playlist.size() != m_queue.rowCount())
        return;
    const QVector<QVariantMap> rows = m_queue.rows();
    QVector<bool> used(rows.size(), false);
    QVector<QVariantMap> ordered;
    for (const QVariant &entry : playlist) {
        const QString file = entry.toMap().value(QStringLiteral("filename")).toString();
        int found = -1;
        for (int i = 0; i < rows.size(); ++i) {
            if (!used[i] && rows.at(i).value(QStringLiteral("path")).toString() == file) {
                found = i;
                break;
            }
        }
        if (found < 0)
            return;
        used[found] = true;
        ordered.append(rows.at(found));
    }
    if (ordered != rows) {
        m_queue.setRows(ordered);
        emit currentChanged();
    }
}
