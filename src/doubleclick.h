#pragma once

#include <QtGlobal>

// Tells a double click from two single ones. Status-notifier tray hosts only report
// plain clicks, so the shell counts them itself.
class DoubleClick
{
public:
    explicit DoubleClick(qint64 intervalMs) : m_interval(intervalMs) {}

    // Register a click at time `nowMs`; true when it is the second of a double click.
    bool click(qint64 nowMs)
    {
        const bool second = m_last >= 0 && nowMs - m_last <= m_interval;
        m_last = second ? -1 : nowMs;   // a double click is used up: a third click starts a new one
        return second;
    }

private:
    qint64 m_interval;
    qint64 m_last = -1;
};
