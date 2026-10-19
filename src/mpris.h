#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QVariantMap>

class Library;
class Player;

// The MPRIS service (media keys, desktop widgets). Registers as
// org.mpris.MediaPlayer2.omamusic on the session bus and follows the Player.
class Mpris : public QObject
{
    Q_OBJECT
public:
    explicit Mpris(Player &player, Library &library,
                   const QString &service = QStringLiteral("org.mpris.MediaPlayer2.omamusic"),
                   QObject *parent = nullptr);

    bool registered() const { return m_registered; }

    // Used by the adaptors.
    Player &player() { return m_player; }
    QVariantMap metadata();
    QString playbackStatus() const;

signals:
    void raiseRequested();
    void quitRequested();

private:
    void emitChanged(const QStringList &names);

    Player &m_player;
    Library &m_library;
    bool m_registered = false;
};
