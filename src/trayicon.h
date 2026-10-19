#pragma once

#include <QByteArray>
#include <QColor>
#include <QIcon>
#include <QImage>

namespace TrayIcon {

// The svg with every fill="#rrggbb" replaced by the given colour.
QByteArray recolor(const QByteArray &svg, const QColor &color);
// The recoloured svg drawn into a transparent size x size image.
QImage render(const QByteArray &svg, const QColor &color, int size);
// An icon with the usual tray sizes.
QIcon make(const QByteArray &svg, const QColor &color);

}
