#pragma once

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QPixmap>
#include <QSize>
#include <QString>

class QAbstractButton;
class QAbstractSpinBox;
class QLabel;
class QWidget;

namespace UiAssets {

QString bodyFontFamily();
QString titleFontFamily();
QFont bodyFont(int pixelSize, int weight = QFont::Medium);
QFont titleFont(int pixelSize);

QString iconPath(const QString &name);
QPixmap svgPixmap(const QString &name, const QSize &logicalSize,
                  const QColor &color = QColor(QStringLiteral("#0B163A")),
                  const QWidget *context = nullptr);
QIcon svgIcon(const QString &name, const QColor &color = QColor(QStringLiteral("#0B163A")),
              const QSize &logicalSize = QSize(24, 24), const QWidget *context = nullptr);
void setButtonIcon(QAbstractButton *button, const QString &name, int logicalSize = 20,
                   const QColor &color = QColor(QStringLiteral("#0B163A")));
QLabel *createIconLabel(QWidget *parent, const QString &name, int logicalSize,
                        const QColor &color = QColor(QStringLiteral("#0B163A")));
void refreshIconLabel(QLabel *label, const QString &name, int logicalSize,
                       const QColor &color = QColor(QStringLiteral("#0B163A")));
void attachFixedUnit(QAbstractSpinBox *spinBox, const QString &unit);

} // namespace UiAssets
