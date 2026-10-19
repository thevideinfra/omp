#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>

// A ten-band graphic equalizer applied as an mpv audio filter. The presets are
// the usual ones; "Flat" is no filter at all.
namespace Equalizer {

constexpr int kBands = 10;

// Band centre frequencies in Hz.
const QList<int> &bandFrequencies();
QStringList presetNames();
// Gains in dB for a preset, one per band; all zero for an unknown name.
QVariantList gains(const QString &preset);
// The mpv --af value for a preset: empty for Flat or an unknown name. Boosts
// are offset by a matching preamp cut so loud tracks do not clip.
QString filter(const QString &preset);

}
