#include "LogoWidget.h"

#include <QPainter>
#include <QSizePolicy>
#include <QSvgRenderer>

LogoWidget::LogoWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMinimumSize(120, 54);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

QSize LogoWidget::sizeHint() const
{
    return QSize(170, 76);
}

void LogoWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    static QSvgRenderer logo(QStringLiteral(":/branding/v5/shanheng-logo-horizontal.svg"));
    logo.setAspectRatioMode(Qt::KeepAspectRatio);
    const QRectF available = QRectF(rect()).adjusted(2.0, 2.0, -2.0, -2.0);
    QSizeF fitted = logo.viewBoxF().size();
    if (fitted.isEmpty())
        fitted = QSizeF(1557.0, 696.0);
    fitted.scale(available.size(), Qt::KeepAspectRatio);
    const QRectF target(available.center().x() - fitted.width() / 2.0,
                        available.center().y() - fitted.height() / 2.0,
                        fitted.width(), fitted.height());
    logo.render(&painter, target);
}
