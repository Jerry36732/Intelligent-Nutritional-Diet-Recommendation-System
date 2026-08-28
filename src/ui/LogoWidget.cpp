#include "LogoWidget.h"

#include <QPainter>
#include <QPainterPath>

LogoWidget::LogoWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(42, 42);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void LogoWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#00C4B3"));
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 13, 13);

    // Plate / bowl
    QPen line(QColor("#fffefa"));
    line.setWidthF(1.8);
    line.setCapStyle(Qt::RoundCap);
    painter.setPen(line);
    painter.setBrush(Qt::NoBrush);
    painter.drawArc(QRectF(10, 20, 22, 12), 15 * 16, 150 * 16);
    painter.drawLine(QPointF(13, 30), QPointF(29, 30));

    // A pair of leaves growing from the centre of the plate.
    QPainterPath stem;
    stem.moveTo(21, 25);
    stem.cubicTo(20.5, 20, 21.5, 16, 25, 12);
    painter.drawPath(stem);

    painter.setBrush(QColor("#dcebdd"));
    QPainterPath rightLeaf;
    rightLeaf.moveTo(22, 18);
    rightLeaf.cubicTo(25, 12, 30, 12, 30, 12);
    rightLeaf.cubicTo(29.5, 17, 26, 19, 22, 18);
    painter.drawPath(rightLeaf);

    QPainterPath leftLeaf;
    leftLeaf.moveTo(20.5, 21);
    leftLeaf.cubicTo(16, 16, 12.5, 17, 11.5, 18);
    leftLeaf.cubicTo(13.5, 22.5, 17, 23, 20.5, 21);
    painter.drawPath(leftLeaf);
}
