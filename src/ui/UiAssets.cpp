#include "UiAssets.h"

#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QEvent>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmapCache>
#include <QScreen>
#include <QSvgRenderer>
#include <QTimer>
#include <QWidget>
#include <QWindow>

#include <algorithm>

namespace {
QString firstAvailable(const QStringList &candidates, const QString &fallback)
{
    const QStringList families = QFontDatabase::families();
    for (const QString &candidate : candidates) {
        const auto it = std::find_if(families.cbegin(), families.cend(),
                                     [&candidate](const QString &family) {
            return family.compare(candidate, Qt::CaseInsensitive) == 0;
        });
        if (it != families.cend())
            return *it;
    }
    return fallback;
}

qreal deviceRatio(const QWidget *context)
{
    if (context && context->windowHandle() && context->windowHandle()->screen())
        return context->windowHandle()->screen()->devicePixelRatio();
    if (QScreen *screen = QApplication::primaryScreen())
        return screen->devicePixelRatio();
    return 1.0;
}

class FixedUnitFilter final : public QObject
{
public:
    FixedUnitFilter(QAbstractSpinBox *spinBox, const QString &unit)
        : QObject(spinBox)
        , m_spinBox(spinBox)
    {
        m_label = new QLabel(unit, spinBox);
        m_label->setObjectName(QStringLiteral("FixedUnitLabel"));
        m_label->setAlignment(Qt::AlignCenter);
        m_label->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_label->show();
        m_label->raise();
        spinBox->installEventFilter(this);
        if (QLineEdit *editor = spinBox->findChild<QLineEdit *>())
            editor->installEventFilter(this);
        updateGeometry();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show
            || event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
            updateGeometry();
        }
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) {
            QTimer::singleShot(0, m_spinBox, [spinBox = m_spinBox]() {
                if (QLineEdit *editor = spinBox->findChild<QLineEdit *>();
                    editor && editor->text() == spinBox->specialValueText()) {
                    editor->clear();
                }
            });
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            QTimer::singleShot(0, m_spinBox, [spinBox = m_spinBox]() {
                if (QLineEdit *editor = spinBox->findChild<QLineEdit *>()) {
                    editor->setFocus(Qt::MouseFocusReason);
                    editor->selectAll();
                }
            });
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void updateGeometry()
    {
        if (!m_spinBox || !m_label)
            return;
        const int width = qMax(32, QFontMetrics(m_spinBox->font()).horizontalAdvance(m_label->text()) + 18);
        m_label->setGeometry(qMax(0, m_spinBox->width() - width - 5), 1,
                             width, qMax(0, m_spinBox->height() - 2));
        m_label->raise();
    }

    QAbstractSpinBox *m_spinBox = nullptr;
    QLabel *m_label = nullptr;
};
} // namespace

namespace UiAssets {

QString bodyFontFamily()
{
    static const QString family = firstAvailable(
        {QStringLiteral("Noto Sans SC"), QStringLiteral("Noto Sans CJK SC"),
         QStringLiteral("Source Han Sans SC"), QStringLiteral("Microsoft YaHei UI"),
         QStringLiteral("Microsoft YaHei"), QStringLiteral("PingFang SC"),
         QStringLiteral("Segoe UI")},
        QStringLiteral("Microsoft YaHei UI"));
    return family;
}

QString titleFontFamily()
{
    static const QString family = firstAvailable(
        {QStringLiteral("STKaiti"), QStringLiteral("华文楷体"), QStringLiteral("KaiTi"),
         QStringLiteral("楷体"), QStringLiteral("Microsoft YaHei UI")},
        bodyFontFamily());
    return family;
}

QFont bodyFont(int pixelSize, int weight)
{
    QFont font(bodyFontFamily());
    font.setPixelSize(pixelSize);
    font.setWeight(static_cast<QFont::Weight>(weight));
    font.setHintingPreference(QFont::PreferFullHinting);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

QFont titleFont(int pixelSize)
{
    QFont font(titleFontFamily());
    font.setPixelSize(pixelSize);
    font.setWeight(QFont::Medium);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0);
    font.setHintingPreference(QFont::PreferFullHinting);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

QString iconPath(const QString &name)
{
    QString fileName = name;
    if (!fileName.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive))
        fileName += QStringLiteral(".svg");
    return QStringLiteral(":/icons/v5/%1").arg(fileName);
}

QPixmap svgPixmap(const QString &name, const QSize &logicalSize, const QColor &color,
                  const QWidget *context)
{
    const qreal ratio = qMin<qreal>(4.0, qMax<qreal>(2.0, deviceRatio(context)));
    const QString cacheKey = QStringLiteral("shanheng-svg:%1:%2x%3:%4:%5")
                                 .arg(name)
                                 .arg(logicalSize.width())
                                 .arg(logicalSize.height())
                                 .arg(color.isValid() ? color.name(QColor::HexArgb)
                                                      : QStringLiteral("original"))
                                 .arg(qRound(ratio * 100.0));
    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached))
        return cached;

    QFile file(iconPath(name));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QByteArray data = file.readAll();
    if (color.isValid()) {
        const QByteArray replacement = color.name(QColor::HexRgb).toUpper().toUtf8();
        data.replace("#0B163A", replacement);
        data.replace("#0b163a", replacement);
    }
    QSvgRenderer renderer(data);
    if (!renderer.isValid())
        return {};
    // 18-26 px 的线性图标若直接按物理像素栅格化，极易出现断线、锯齿和缺笔。
    // 无论屏幕 DPR 为多少，至少以 2 倍尺寸渲染，再通过 DPR 映射回逻辑尺寸。
    renderer.setAspectRatioMode(Qt::KeepAspectRatio);
    const QSize physical(qMax(1, qRound(logicalSize.width() * ratio)),
                         qMax(1, qRound(logicalSize.height() * ratio)));
    QPixmap pixmap(physical);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(physical)));
    painter.end();
    pixmap.setDevicePixelRatio(ratio);
    QPixmapCache::insert(cacheKey, pixmap);
    return pixmap;
}

QIcon svgIcon(const QString &name, const QColor &color, const QSize &logicalSize,
              const QWidget *context)
{
    return QIcon(svgPixmap(name, logicalSize, color, context));
}

void setButtonIcon(QAbstractButton *button, const QString &name, int logicalSize,
                   const QColor &color)
{
    if (!button)
        return;
    const QSize size(logicalSize, logicalSize);
    button->setIcon(svgIcon(name, color, size, button));
    button->setIconSize(size);
}

QLabel *createIconLabel(QWidget *parent, const QString &name, int logicalSize,
                        const QColor &color)
{
    auto *label = new QLabel(parent);
    label->setFixedSize(logicalSize, logicalSize);
    label->setAlignment(Qt::AlignCenter);
    refreshIconLabel(label, name, logicalSize, color);
    return label;
}

void refreshIconLabel(QLabel *label, const QString &name, int logicalSize, const QColor &color)
{
    if (!label)
        return;
    label->setPixmap(svgPixmap(name, QSize(logicalSize, logicalSize), color, label));
}

void attachFixedUnit(QAbstractSpinBox *spinBox, const QString &unit)
{
    if (!spinBox || unit.trimmed().isEmpty())
        return;
    spinBox->setProperty("fixedUnit", true);
    new FixedUnitFilter(spinBox, unit.trimmed());
}

} // namespace UiAssets
