#ifndef LOGOWIDGET_H
#define LOGOWIDGET_H

#include <QWidget>

// A small original mark: a bowl-shaped plate cradling two sprouting leaves.
// It stays crisp at any desktop DPI without needing an external image asset.
class LogoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LogoWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // LOGOWIDGET_H
