#ifndef RECIPEIMAGEPROVIDER_H
#define RECIPEIMAGEPROVIDER_H

#include <QPixmap>
#include <QString>

class RecipeImageProvider
{
public:
    static QString imagePath(const QString &recipeName);
    static QPixmap pixmap(const QString &recipeName, const QSize &size);
};

#endif // RECIPEIMAGEPROVIDER_H
