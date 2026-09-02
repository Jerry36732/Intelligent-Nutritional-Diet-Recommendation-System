#ifndef LOGOWIDGET_H
#define LOGOWIDGET_H

#include <QWidget>

// Canonical renderer for the approved full brand lock-up.  Every screen uses this
// one component so the mark, Chinese name and English subtitle cannot drift apart.
class LogoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LogoWidget(QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

};

#endif // LOGOWIDGET_H
