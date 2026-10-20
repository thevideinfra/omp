#include "scrobbletracker.h"

QString ScrobbleTracker::idOf(const QString &path)
{
    return path.startsWith(QLatin1String("subsonic:")) ? path.mid(9) : QString();
}

void ScrobbleTracker::update(const QString &path, double position, double duration)
{
    const bool changed = path != m_path;
    // The same song starting over: the last position was at its end, this one is at its start.
    const bool looped = !changed && m_submitted && m_last > 3 && duration > 0 && m_last >= duration - 3 && position < 3;
    if (changed || looped) {
        m_path = path;
        m_submitted = false;
        m_last = position;
        if (!idOf(path).isEmpty())
            emit nowPlaying(idOf(path));
        return;
    }
    m_last = position;
    if (m_submitted || idOf(path).isEmpty() || duration < 30)
        return;
    if (position >= qMin(duration / 2, 240.0)) {
        m_submitted = true;
        emit submit(idOf(path));
    }
}
