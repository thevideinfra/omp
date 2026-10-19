#include "trayicon.h"

#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QSvgRenderer>

namespace TrayIcon {

QByteArray recolor(const QByteArray &svg, const QColor &color)
{
    static const QRegularExpression fill(QStringLiteral("fill=\"#[0-9a-fA-F]{6}\""));
    QString text = QString::fromUtf8(svg);
    text.replace(fill, QStringLiteral("fill=\"%1\"").arg(color.name()));
    return text.toUtf8();
}

QImage render(const QByteArray &svg, const QColor &color, int size)
{
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QSvgRenderer renderer(recolor(svg, color));
    QPainter p(&img);
    renderer.render(&p);
    return img;
}

QIcon make(const QByteArray &svg, const QColor &color)
{
    QIcon icon;
    for (int size : {16, 22, 32, 48, 64})
        icon.addPixmap(QPixmap::fromImage(render(svg, color, size)));
    return icon;
}

}
