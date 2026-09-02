#include "RecipeImageProvider.h"

#include "RecipeText.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QReadWriteLock>
#include <QStandardPaths>

namespace {
QHash<QString, QString> &imageMap()
{
    static QHash<QString, QString> map;
    return map;
}

QReadWriteLock &mapLock()
{
    static QReadWriteLock lock;
    return lock;
}

bool &loaded()
{
    static bool value = false;
    return value;
}

QStringList manifestCandidates()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    return {
        appDir + QStringLiteral("/data/recipe_images.json"),
        appDir + QStringLiteral("/../data/recipe_images.json"),
        appDir + QStringLiteral("/../../data/recipe_images.json"),
        QDir::current().absoluteFilePath(QStringLiteral("data/recipe_images.json")),
        QDir::current().absoluteFilePath(
            QStringLiteral("食谱数据/图片工作区/完整食谱配图清单.json")),
    };
}

void ensureLoaded()
{
    QWriteLocker guard(&mapLock());
    if (loaded())
        return;
    loaded() = true;
    for (const QString &candidate : manifestCandidates()) {
        QFile file(candidate);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        const QJsonArray items = doc.object().value(QStringLiteral("items")).toArray();
        const QDir base = QFileInfo(candidate).absoluteDir();
        for (const QJsonValue &value : items) {
            const QJsonObject item = value.toObject();
            const QString name = RecipeText::normalizeName(item.value(QStringLiteral("name")).toString());
            QString path = item.value(QStringLiteral("local_path")).toString();
            if (name.isEmpty() || path.isEmpty())
                continue;
            QFileInfo info(path);
            if (!info.isAbsolute())
                info = QFileInfo(base.absoluteFilePath(path));
            if (!info.exists()) {
                const QString deployed = base.absoluteFilePath(
                    QStringLiteral("recipe_images/%1").arg(QFileInfo(path).fileName()));
                info = QFileInfo(deployed);
            }
            if (info.exists())
                imageMap().insert(name, info.absoluteFilePath());
        }
        if (!imageMap().isEmpty())
            break;
    }
}

QPixmap fallbackPixmap(const QString &name, const QSize &size)
{
    const QSize canvasSize = size.isValid() ? size : QSize(96, 64);
    QPixmap canvas(canvasSize);
    canvas.fill(QColor(QStringLiteral("#EEF5F1")));
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(QStringLiteral("#B9D8C9")), 1));
    painter.drawRoundedRect(canvas.rect().adjusted(0, 0, -1, -1), 7, 7);
    painter.setPen(QColor(QStringLiteral("#0AA66D")));
    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(qMax(11, canvasSize.height() / 4));
    painter.setFont(font);
    const QString label = name.trimmed().isEmpty() ? QStringLiteral("膳") : name.trimmed().left(1);
    painter.drawText(canvas.rect(), Qt::AlignCenter, label);
    return canvas;
}

QColor meanColor(const QImage &image, const QRect &area)
{
    qint64 red = 0;
    qint64 green = 0;
    qint64 blue = 0;
    qint64 alpha = 0;
    int count = 0;
    for (int y = area.top(); y <= area.bottom(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = area.left(); x <= area.right(); ++x) {
            const QRgb pixel = line[x];
            red += qRed(pixel);
            green += qGreen(pixel);
            blue += qBlue(pixel);
            alpha += qAlpha(pixel);
            ++count;
        }
    }
    return count > 0 ? QColor(red / count, green / count, blue / count, alpha / count)
                     : QColor();
}

int colorSpread(const QColor &left, const QColor &right)
{
    return qMax(qMax(qAbs(left.red() - right.red()), qAbs(left.green() - right.green())),
                qMax(qAbs(left.blue() - right.blue()), qAbs(left.alpha() - right.alpha())));
}

QRect visibleContentRect(const QImage &original)
{
    if (original.width() < 24 || original.height() < 24)
        return original.rect();
    const QImage image = original.convertToFormat(QImage::Format_ARGB32);
    const int patch = qBound(2, qMin(image.width(), image.height()) / 32, 16);
    const QList<QColor> corners = {
        meanColor(image, QRect(0, 0, patch, patch)),
        meanColor(image, QRect(image.width() - patch, 0, patch, patch)),
        meanColor(image, QRect(0, image.height() - patch, patch, patch)),
        meanColor(image, QRect(image.width() - patch, image.height() - patch, patch, patch)),
    };
    int cornerVariation = 0;
    for (int i = 0; i < corners.size(); ++i) {
        for (int j = i + 1; j < corners.size(); ++j)
            cornerVariation = qMax(cornerVariation, colorSpread(corners.at(i), corners.at(j)));
    }
    // 四角差异明显通常意味着图片本身已经铺满，不进行内容裁边。
    if (cornerVariation > 28)
        return image.rect();

    int backgroundRed = 0;
    int backgroundGreen = 0;
    int backgroundBlue = 0;
    int backgroundAlpha = 0;
    for (const QColor &corner : corners) {
        backgroundRed += corner.red();
        backgroundGreen += corner.green();
        backgroundBlue += corner.blue();
        backgroundAlpha += corner.alpha();
    }
    backgroundRed /= corners.size();
    backgroundGreen /= corners.size();
    backgroundBlue /= corners.size();
    backgroundAlpha /= corners.size();

    int left = image.width();
    int top = image.height();
    int right = -1;
    int bottom = -1;
    for (int y = 0; y < image.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = line[x];
            const int redDiff = qAbs(qRed(pixel) - backgroundRed);
            const int greenDiff = qAbs(qGreen(pixel) - backgroundGreen);
            const int blueDiff = qAbs(qBlue(pixel) - backgroundBlue);
            const int alphaDiff = qAbs(qAlpha(pixel) - backgroundAlpha);
            const int strongest = qMax(qMax(redDiff, greenDiff), qMax(blueDiff, alphaDiff));
            if (strongest <= 20 && redDiff + greenDiff + blueDiff + alphaDiff <= 42)
                continue;
            left = qMin(left, x);
            top = qMin(top, y);
            right = qMax(right, x);
            bottom = qMax(bottom, y);
        }
    }
    if (right < left || bottom < top)
        return image.rect();

    QRect content(QPoint(left, top), QPoint(right, bottom));
    if (content.width() < 12 || content.height() < 12)
        return image.rect();
    if (content.width() >= image.width() * 0.94
        && content.height() >= image.height() * 0.94)
        return image.rect();

    const int padding = qMax(4, qMin(content.width(), content.height()) / 25);
    return content.adjusted(-padding, -padding, padding, padding).intersected(image.rect());
}
} // namespace

QString RecipeImageProvider::imagePath(const QString &recipeName)
{
    ensureLoaded();
    const QString normalized = RecipeText::normalizeName(recipeName);
    if (normalized == QStringLiteral("燕麦片"))
        return QStringLiteral(":/images/ingredients/oatmeal.png");
    QReadLocker guard(&mapLock());
    return imageMap().value(normalized);
}

QPixmap RecipeImageProvider::pixmap(const QString &recipeName, const QSize &size)
{
    const QString path = imagePath(recipeName);
    const QString cacheKey = QStringLiteral("shanheng-recipe:content-crop-v2:%1:%2x%3")
                                 .arg(path.isEmpty() ? RecipeText::normalizeName(recipeName) : path)
                                 .arg(size.width())
                                 .arg(size.height());
    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached))
        return cached;
    QPixmap source(path);
    if (source.isNull()) {
        QPixmap fallback = fallbackPixmap(recipeName, size);
        QPixmapCache::insert(cacheKey, fallback);
        return fallback;
    }
    const QRect contentRect = visibleContentRect(source.toImage());
    if (contentRect != source.rect())
        source = source.copy(contentRect);
    QPixmap scaled = source.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    if (scaled.size() != size) {
        const int x = qMax(0, (scaled.width() - size.width()) / 2);
        const int y = qMax(0, (scaled.height() - size.height()) / 2);
        scaled = scaled.copy(x, y, size.width(), size.height());
    }
    QPixmap rounded(size);
    rounded.fill(Qt::transparent);
    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(size)), 7, 7);
    painter.setClipPath(clip);
    painter.drawPixmap(0, 0, scaled);
    painter.end();
    QPixmapCache::insert(cacheKey, rounded);
    return rounded;
}
