#include "IconFactory.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace obor::IconFactory {

namespace {

constexpr int kSize = 64; // logical drawing grid
constexpr qreal kScale = 2.0;

void drawPlay(QPainter &p, const QColor &c)
{
    p.setBrush(c);
    p.drawPolygon(QPolygonF({QPointF(20, 12), QPointF(52, 32), QPointF(20, 52)}));
}

void drawPause(QPainter &p, const QColor &c)
{
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawRoundedRect(QRectF(16, 12, 11, 40), 3, 3);
    p.drawRoundedRect(QRectF(37, 12, 11, 40), 3, 3);
}

void drawStop(QPainter &p, const QColor &c)
{
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawRoundedRect(QRectF(15, 15, 34, 34), 5, 5);
}

void drawPreview(QPainter &p, const QColor &c)
{
    QPainterPath eye;
    eye.moveTo(6, 32);
    eye.quadTo(32, 4, 58, 32);
    eye.quadTo(32, 60, 6, 32);
    p.drawPath(eye);
    p.setBrush(c);
    p.drawEllipse(QPointF(32, 32), 7, 7);
}

void drawFolder(QPainter &p, const QColor &)
{
    QPainterPath path;
    path.moveTo(8, 50);
    path.lineTo(8, 14);
    path.lineTo(26, 14);
    path.lineTo(31, 21);
    path.lineTo(56, 21);
    path.lineTo(56, 50);
    path.closeSubpath();
    p.drawPath(path);
}

void drawAdd(QPainter &p, const QColor &)
{
    p.drawLine(QPointF(32, 12), QPointF(32, 52));
    p.drawLine(QPointF(12, 32), QPointF(52, 32));
}

void drawDuplicate(QPainter &p, const QColor &)
{
    p.drawRoundedRect(QRectF(22, 22, 32, 32), 5, 5);
    QPainterPath back;
    back.moveTo(14, 42);
    back.lineTo(10, 42);
    back.lineTo(10, 10);
    back.lineTo(42, 10);
    back.lineTo(42, 14);
    p.drawPath(back);
}

void drawEdit(QPainter &p, const QColor &)
{
    p.drawPolygon(QPolygonF({QPointF(12, 52), QPointF(15, 40), QPointF(42, 13), QPointF(51, 22),
                             QPointF(24, 49)}));
    p.drawLine(QPointF(36, 19), QPointF(45, 28));
}

void drawTrash(QPainter &p, const QColor &)
{
    p.drawLine(QPointF(12, 17), QPointF(52, 17));
    p.drawLine(QPointF(26, 10), QPointF(38, 10));
    p.drawPolyline(QPolygonF({QPointF(17, 17), QPointF(20, 54), QPointF(44, 54), QPointF(47, 17)}));
    p.drawLine(QPointF(28, 27), QPointF(28, 44));
    p.drawLine(QPointF(36, 27), QPointF(36, 44));
}

void drawHistory(QPainter &p, const QColor &)
{
    p.drawEllipse(QPointF(32, 32), 23, 23);
    p.drawPolyline(QPolygonF({QPointF(32, 18), QPointF(32, 33), QPointF(42, 40)}));
}

void drawInfo(QPainter &p, const QColor &c)
{
    p.drawEllipse(QPointF(32, 32), 24, 24);
    p.drawLine(QPointF(32, 29), QPointF(32, 45));
    p.setBrush(c);
    p.drawEllipse(QPointF(32, 20), 2, 2);
}

void drawWarning(QPainter &p, const QColor &c)
{
    p.drawPolygon(QPolygonF({QPointF(32, 8), QPointF(58, 54), QPointF(6, 54)}));
    p.drawLine(QPointF(32, 24), QPointF(32, 38));
    p.setBrush(c);
    p.drawEllipse(QPointF(32, 46), 2, 2);
}

void drawError(QPainter &p, const QColor &)
{
    p.drawEllipse(QPointF(32, 32), 24, 24);
    p.drawLine(QPointF(23, 23), QPointF(41, 41));
    p.drawLine(QPointF(41, 23), QPointF(23, 41));
}

void drawCheck(QPainter &p, const QColor &)
{
    p.drawPolyline(QPolygonF({QPointF(12, 34), QPointF(26, 48), QPointF(52, 18)}));
}

void drawUpdateMode(QPainter &p, const QColor &)
{
    p.drawRoundedRect(QRectF(34, 10, 22, 44), 4, 4);
    p.drawLine(QPointF(6, 32), QPointF(28, 32));
    p.drawPolyline(QPolygonF({QPointF(19, 22), QPointF(29, 32), QPointF(19, 42)}));
    p.drawLine(QPointF(45, 26), QPointF(45, 38));
    p.drawLine(QPointF(39, 32), QPointF(51, 32));
}

void drawMirrorMode(QPainter &p, const QColor &)
{
    const QRectF r(12, 12, 40, 40);
    p.drawArc(r, 30 * 16, 150 * 16);
    p.drawArc(r, 210 * 16, 150 * 16);
    p.drawPolyline(QPolygonF({QPointF(41, 11), QPointF(50, 21), QPointF(38, 24)}));
    p.drawPolyline(QPolygonF({QPointF(23, 53), QPointF(14, 43), QPointF(26, 40)}));
}

void drawDocument(QPainter &p, const QColor &)
{
    p.drawPolygon(QPolygonF({QPointF(14, 8), QPointF(38, 8), QPointF(50, 20), QPointF(50, 56),
                             QPointF(14, 56)}));
    p.drawLine(QPointF(22, 30), QPointF(42, 30));
    p.drawLine(QPointF(22, 39), QPointF(42, 39));
    p.drawLine(QPointF(22, 48), QPointF(34, 48));
}

void drawArrow(QPainter &p, const QColor &)
{
    p.drawLine(QPointF(10, 32), QPointF(52, 32));
    p.drawPolyline(QPolygonF({QPointF(38, 18), QPointF(52, 32), QPointF(38, 46)}));
}

using DrawFn = void (*)(QPainter &, const QColor &);

DrawFn drawFunction(IconKind kind)
{
    static const DrawFn table[] = {drawPlay,     drawPause,      drawStop,       drawPreview,
                                   drawFolder,   drawAdd,        drawDuplicate,  drawEdit,
                                   drawTrash,    drawHistory,    drawInfo,       drawWarning,
                                   drawError,    drawCheck,      drawUpdateMode, drawMirrorMode,
                                   drawDocument, drawArrow};
    return table[int(kind)];
}

QPixmap canvas()
{
    QPixmap pixmap(int(kSize * kScale), int(kSize * kScale));
    pixmap.setDevicePixelRatio(kScale);
    pixmap.fill(Qt::transparent);
    return pixmap;
}

} // namespace

QIcon icon(IconKind kind, const QColor &color)
{
    // The neutral default reads well on light and dark backgrounds, so icons
    // created before a theme switch stay visible.
    const QColor c = color.isValid() ? color : QColor(0x7C, 0x87, 0x99);
    QPixmap pixmap = canvas();
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(c, 5.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    drawFunction(kind)(p, c);
    p.end();
    return QIcon(pixmap);
}

QIcon dot(const QColor &color)
{
    QPixmap pixmap = canvas();
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(QPointF(32, 32), 15, 15);
    p.end();
    return QIcon(pixmap);
}

QIcon appIcon()
{
    return QIcon(QStringLiteral(":/icons/oborcopy.png"));
}

} // namespace obor::IconFactory
