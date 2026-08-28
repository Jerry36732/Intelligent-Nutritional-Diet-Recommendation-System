#include "RecipeText.h"

#include <QRegularExpression>

namespace RecipeText {

QString normalizeName(QString name)
{
    name = name.trimmed();
    static const QString suffix = QStringLiteral("的做法");
    if (name.endsWith(suffix))
        name = name.left(name.length() - suffix.length()).trimmed();
    static const QString suffix2 = QStringLiteral("做法");
    if (name.endsWith(suffix2) && name.length() > suffix2.length())
        name = name.left(name.length() - suffix2.length()).trimmed();
    name.remove(QRegularExpression(
        QStringLiteral("^(?:(?:减肥|减脂|增肌|养生|保健|营养)?菜谱\\s*\\d*|健康减重|健康减脂)[_－—–-]+\\s*")));
    name.remove(QRegularExpression(QStringLiteral("^[^－—–-]+[－—–-]+\\s*")));
    name.remove(QRegularExpression(
        QStringLiteral("\\s*[（(][^）)]*(?:个?月|月龄|周岁|岁)[^）)]*[）)]\\s*$")));
    return name;
}

} // namespace RecipeText
