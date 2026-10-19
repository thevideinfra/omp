#include "equalizer.h"

#include <algorithm>

#include <QMap>

namespace Equalizer {

namespace {

struct Preset
{
    const char *name;
    double gains[kBands];
};

// 31 62 125 250 500 1k 2k 4k 8k 16k
const Preset kPresets[] = {
    {"Flat",         {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"Bass Boost",   {5, 4, 3, 1.5, 0, 0, 0, 0, 0, 0}},
    {"Treble Boost", {0, 0, 0, 0, 0, 1, 2.5, 4, 5, 5.5}},
    {"Rock",         {5, 4, 3, 1, -1, -1, 0, 2, 3, 4}},
    {"Pop",          {-1, 1, 3, 4, 3, 0, -1, -1, 1, 2}},
    {"Jazz",         {3, 2, 1, 2, -2, -2, 0, 1.5, 3, 3.5}},
    {"Classical",    {4, 3, 2, 2, -1, -1, 0, 2, 3, 3}},
    {"Vocal",        {-2, -3, -3, 1, 3.5, 3.5, 3, 1.5, 0, -1}},
    {"Dance",        {5, 4, 2, 0, 0, -2, -3, -3, 0, 0}},
    {"Loudness",     {5, 3, 0, 0, -1, 0, -1, 0, 3, 1}},
};

const Preset *find(const QString &name)
{
    for (const Preset &p : kPresets) {
        if (name == QLatin1String(p.name))
            return &p;
    }
    return nullptr;
}

}

const QList<int> &bandFrequencies()
{
    static const QList<int> f{31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    return f;
}

QStringList presetNames()
{
    QStringList names;
    for (const Preset &p : kPresets)
        names << QLatin1String(p.name);
    return names;
}

QVariantList gains(const QString &preset)
{
    QVariantList out;
    const Preset *p = find(preset);
    for (int i = 0; i < kBands; ++i)
        out << (p ? p->gains[i] : 0.0);
    return out;
}

QString filter(const QString &preset)
{
    const Preset *p = find(preset);
    if (!p)
        return {};
    const double top = *std::max_element(p->gains, p->gains + kBands);
    QStringList parts;
    if (top > 0)
        parts << QStringLiteral("volume=-%1dB").arg(top);
    for (int i = 0; i < kBands; ++i) {
        if (p->gains[i] != 0)
            parts << QStringLiteral("equalizer=f=%1:t=o:w=1:g=%2").arg(bandFrequencies().at(i)).arg(p->gains[i]);
    }
    if (parts.isEmpty())
        return {};
    return QStringLiteral("lavfi=[%1]").arg(parts.join(','));
}

}
