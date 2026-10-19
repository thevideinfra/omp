#pragma once

#include <functional>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include "mpvclient.h"
#include "rowsmodel.h"

class Player : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)
    Q_PROPERTY(RowsModel *queue READ queue CONSTANT)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY shuffleChanged)
    Q_PROPERTY(QString repeat READ repeat WRITE setRepeat NOTIFY repeatChanged)
    Q_PROPERTY(QString eqPreset READ eqPreset WRITE setEqPreset NOTIFY eqPresetChanged)
    Q_PROPERTY(QStringList eqPresets READ eqPresets CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit Player(const QString &mpvProgram = QStringLiteral("mpv"),
                    const QStringList &extraArgs = {}, const QString &socketPath = {},
                    QObject *parent = nullptr);
    ~Player() override;

    void start();

    bool ready() const { return m_ready; }
    bool playing() const { return m_playing; }
    double position() const { return m_position; }
    double duration() const { return m_duration; }
    int volume() const { return m_volume; }
    int currentIndex() const { return m_index; }
    QVariantMap current() const;
    RowsModel *queue() { return &m_queue; }
    QString error() const { return m_error; }

    void setVolume(int volume);

    bool shuffle() const { return m_shuffle; }
    // Shuffles the songs after the playing one; off puts them back in order.
    void setShuffle(bool on);
    // "off", "all" (repeat the queue) or "one" (repeat the song).
    QString repeat() const { return m_repeat; }
    void setRepeat(const QString &mode);
    Q_INVOKABLE void cycleRepeat();
    QString eqPreset() const { return m_eqPreset; }
    void setEqPreset(const QString &preset);
    QStringList eqPresets() const;
    Q_INVOKABLE QVariantList eqGains(const QString &preset) const;
    // Applied when mpv starts; set before start().
    void setStartPlayback(bool shuffle, const QString &repeat, const QString &eqPreset);
    // The volume mpv starts (and restarts) at; set before start().
    void setStartVolume(int volume) { m_startVolume = qBound(0, volume, 130); }

    Q_INVOKABLE void playTracks(const QVariantList &tracks, int startIndex = 0);
    Q_INVOKABLE void enqueue(const QVariantMap &track);
    Q_INVOKABLE void enqueueTracks(const QVariantList &tracks);
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seekBy(double seconds);
    Q_INVOKABLE void seekTo(double seconds);
    Q_INVOKABLE void jumpTo(int index);
    Q_INVOKABLE void clearQueue();

signals:
    void readyChanged();
    void playingChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void currentIndexChanged();
    void currentChanged();
    void errorChanged();
    void shuffleChanged();
    void repeatChanged();
    void eqPresetChanged();

private:
    void onConnected();
    void onProperty(const QString &name, const QVariant &value);
    void onProcessFinished();
    void setReady(bool ready);
    void setError(const QString &error);
    void updatePlaying();
    void applyRepeat();
    void applyEqualizer();
    void reorderUpcoming(const std::function<void(QVector<QVariantMap> &)> &order);

    QString m_program;
    QStringList m_extraArgs;
    QString m_socketPath;

    QProcess m_proc;
    MpvClient m_client;
    QTimer m_retry;
    int m_attempts = 0;
    int m_restarts = 0;
    bool m_shuttingDown = false;

    RowsModel m_queue;
    bool m_ready = false;
    bool m_paused = false;
    bool m_idle = true;
    bool m_playing = false;
    double m_position = 0;
    double m_duration = 0;
    int m_volume = 100;
    int m_startVolume = 100;
    int m_index = -1;
    bool m_shuffle = false;
    QString m_repeat = QStringLiteral("off");
    QString m_eqPreset = QStringLiteral("Flat");
    QString m_error;
};
