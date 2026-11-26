#include "icons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPalette>

#include <functional>

namespace Icons {
namespace {

// Draws `shape` on a 48px canvas centred on the origin, once for the normal
// state and once for the disabled one.
QIcon draw(const QPalette &palette, qreal penWidth, const std::function<void(QPainter &)> &shape) {
    QIcon icon;
    const std::pair<QIcon::Mode, QPalette::ColorGroup> modes[] = {{QIcon::Normal, QPalette::Active},
                                                                   {QIcon::Disabled, QPalette::Disabled}};
    for (const auto &[mode, group] : modes) {
        QPixmap pixmap(48, 48);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(palette.color(group, QPalette::ButtonText), penWidth, Qt::SolidLine, Qt::RoundCap,
                            Qt::RoundJoin));
        painter.translate(24, 24);
        shape(painter);
        painter.end();
        icon.addPixmap(pixmap, mode);
    }
    return icon;
}

} // namespace

QIcon arrow(Qt::ArrowType direction, const QPalette &palette) {
    return draw(palette, 4.5, [direction](QPainter &p) {
        // Drawn pointing left, then turned to face the requested direction.
        p.rotate(direction == Qt::RightArrow ? 180 : direction == Qt::UpArrow ? 90
                                                    : direction == Qt::DownArrow ? 270 : 0);
        p.drawLine(QPointF(-14, 0), QPointF(14, 0));
        p.drawPolyline(QPolygonF({QPointF(-2, -12), QPointF(-14, 0), QPointF(-2, 12)}));
    });
}

QIcon plus(const QPalette &palette) {
    return draw(palette, 4.5, [](QPainter &p) {
        p.drawLine(QPointF(-14, 0), QPointF(14, 0));
        p.drawLine(QPointF(0, -14), QPointF(0, 14));
    });
}

QIcon close(const QPalette &palette) {
    return draw(palette, 5, [](QPainter &p) {
        p.drawLine(QPointF(-12, -12), QPointF(12, 12));
        p.drawLine(QPointF(-12, 12), QPointF(12, -12));
    });
}

QIcon chevron(const QPalette &palette) {
    return draw(palette, 4.5, [](QPainter &p) {
        p.drawPolyline(QPolygonF({QPointF(-14, -6), QPointF(0, 8), QPointF(14, -6)}));
    });
}

QIcon padlock(const QPalette &palette) {
    return draw(palette, 4, [](QPainter &p) {
        p.drawRoundedRect(QRectF(-13, -3, 26, 20), 3, 3);
        QPainterPath shackle;
        shackle.moveTo(-8, -3);
        shackle.lineTo(-8, -10);
        shackle.arcTo(QRectF(-8, -19, 16, 18), 180, -180);
        shackle.lineTo(8, -3);
        p.drawPath(shackle);
    });
}

QIcon link(const QPalette &palette) {
    return draw(palette, 4.5, [](QPainter &p) {
        // Two rings joined at an angle.
        p.rotate(-45);
        p.drawRoundedRect(QRectF(-19, -6, 20, 12), 6, 6);
        p.drawRoundedRect(QRectF(-1, -6, 20, 12), 6, 6);
    });
}

QIcon star(const QPalette &palette) {
    QIcon icon;
    const std::pair<QIcon::Mode, QColor> modes[] = {
        {QIcon::Normal, palette.color(QPalette::Accent)},
        {QIcon::Disabled, palette.color(QPalette::Disabled, QPalette::ButtonText)}};
    for (const auto &[mode, color] : modes) {
        QPixmap pixmap(48, 48);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(24, 25);
        QPolygonF points;
        for (int i = 0; i < 10; ++i) {
            QTransform turn;
            turn.rotate(36 * i);
            points << turn.map(QPointF(0, i % 2 ? -9 : -20));
        }
        p.setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(color);
        p.drawPolygon(points);
        p.end();
        icon.addPixmap(pixmap, mode);
    }
    return icon;
}

} // namespace Icons
