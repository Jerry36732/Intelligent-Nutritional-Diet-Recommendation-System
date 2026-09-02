#include "RecipeText.h"

#include <QHash>
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

QString normalizeIngredientName(QString name)
{
    name = name.trimmed();
    name.replace(QStringLiteral("咖哩"), QStringLiteral("咖喱"));
    name.remove(QRegularExpression(QStringLiteral("^(?:调味料|配料)\\s*[:：]\\s*")));
    name.remove(QRegularExpression(QStringLiteral("^[碎切]")));
    name.remove(QRegularExpression(QStringLiteral("^微量调料\\s*[:：]\\s*")));
    name.remove(QRegularExpression(QStringLiteral("[（(]\\s*\\d+(?:\\.\\d+)?\\s*(?:个|只|枚|片|块|根|条|朵|瓣|棵)[）)]\\s*$")));

    // 数量和原文已在 quantity/source_text 中独立保存；名称只保留食材本身。
    name.remove(QRegularExpression(
        QStringLiteral("(?:约)?(?:\\d+(?:\\.\\d+)?(?:[/／]\\d+)?|[一二两三四五六七八九十半]+)"
                       "(?:约)?\\s*(?:小块|中匙|小勺|小匙|茶勺|茶匙|汤勺|汤匙|大匙|"
                       "匙|棵|粒|杯|个|只|枚|片|块|根|条|朵|瓣|张)?(?:半)?[~～/／]?\\s*$")));
    name.remove(QRegularExpression(
        QStringLiteral("[lI](?:小勺|小匙|茶勺|茶匙|汤勺|汤匙|大匙|匙)\\s*$")));
    name.remove(QRegularExpression(
        QStringLiteral("(?:小勺|小匙|茶勺|茶匙|汤勺|汤匙|大匙|匙)\\s*$")));
    name.remove(QRegularExpression(QStringLiteral("(?:公斤|千克|kg)\\s*$"),
                                   QRegularExpression::CaseInsensitiveOption));
    name.remove(QRegularExpression(QStringLiteral("(?:约|各)\\s*$")));

    const QHash<QString, QString> exact = {
        {QStringLiteral("猪蹄约克生姜"), QStringLiteral("生姜")},
        {QStringLiteral("黄姜粉或咖喱粉"), QStringLiteral("咖喱粉")},
        {QStringLiteral("蒜末"), QStringLiteral("大蒜")},
        {QStringLiteral("蒜泥"), QStringLiteral("大蒜")},
        {QStringLiteral("葱花"), QStringLiteral("香葱")},
        {QStringLiteral("姜数片磨鼓半汤匙"), QStringLiteral("姜、豆豉")},
        {QStringLiteral("姜数片磨鼓"), QStringLiteral("姜、豆豉")},
    };
    name = exact.value(name, name);
    return name.trimmed();
}

} // namespace RecipeText
