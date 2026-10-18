#pragma once

#include <cmath>

#include <QDataStream>
#include <QFile>
#include <QString>

// Writes a mono 16-bit 8 kHz sine WAV of the given length and returns its path.
inline QString writeWav(const QString &path, double seconds)
{
    const int rate = 8000;
    const int samples = int(rate * seconds);
    QByteArray pcm(samples * 2, 0);
    auto *s = reinterpret_cast<qint16 *>(pcm.data());
    for (int i = 0; i < samples; ++i)
        s[i] = qint16(8000 * std::sin(2 * M_PI * 440 * i / rate));

    QByteArray header;
    QDataStream d(&header, QIODevice::WriteOnly);
    d.setByteOrder(QDataStream::LittleEndian);
    d.writeRawData("RIFF", 4);
    d << quint32(36 + pcm.size());
    d.writeRawData("WAVEfmt ", 8);
    d << quint32(16) << quint16(1) << quint16(1) << quint32(rate) << quint32(rate * 2)
      << quint16(2) << quint16(16);
    d.writeRawData("data", 4);
    d << quint32(pcm.size());

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return {};
    f.write(header);
    f.write(pcm);
    return path;
}
