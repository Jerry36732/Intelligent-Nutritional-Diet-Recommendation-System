#include "ShoppingListService.h"

#include "../dao/FridgeDAO.h"
#include "../dao/RecipeDAO.h"
#include "../entities/FridgeItem.h"

#include <QDateTime>
#include <QHash>
#include <QRegularExpression>
#include <QtMath>

#include <algorithm>

namespace {
struct Aggregate
{
    QString displayName;
    double planned = 0.0;
    double fridge = 0.0;
};

bool nearName(const QString &left, const QString &right)
{
    if (left == right)
        return true;
    if (left.size() >= 2 && right.size() >= 2
        && (left.contains(right) || right.contains(left)))
        return true;
    const QList<QStringList> families = {
        {QStringLiteral("鸡肉"), QStringLiteral("鸡腿"), QStringLiteral("鸡胸肉"), QStringLiteral("鸡")},
        {QStringLiteral("猪肉"), QStringLiteral("瘦肉"), QStringLiteral("肉末"), QStringLiteral("肉馅")},
        {QStringLiteral("番茄"), QStringLiteral("西红柿")},
        {QStringLiteral("土豆"), QStringLiteral("马铃薯")},
        {QStringLiteral("米饭"), QStringLiteral("白米饭")},
        {QStringLiteral("鸡蛋"), QStringLiteral("全蛋")},
    };
    for (const QStringList &family : families) {
        bool hasLeft = false;
        bool hasRight = false;
        for (const QString &term : family) {
            hasLeft = hasLeft || left.contains(term);
            hasRight = hasRight || right.contains(term);
        }
        if (hasLeft && hasRight)
            return true;
    }
    return false;
}

QString formatGrams(double grams)
{
    if (grams >= 1000.0)
        return QStringLiteral("%1 kg").arg(grams / 1000.0, 0, 'f', grams >= 10000.0 ? 0 : 1);
    return QStringLiteral("%1 g").arg(qMax(1, int(qCeil(grams))));
}
} // namespace

QString ShoppingListService::normalizedIngredientName(const QString &name)
{
    QString value = name.trimmed();
    value.remove(QRegularExpression(QStringLiteral("[（(\\[].*?[）)\\]]")));
    value.remove(QRegularExpression(QStringLiteral("^(?:鲜|净|熟|干|水发)")));
    value.remove(QRegularExpression(
        QStringLiteral("(?:适量|少许|若干|一小块|一大块|小块|大块|薄片|小片|末|丝|片|丁|块|段|泥|茸|粒|条)$")));
    value.replace(QStringLiteral("西红柿"), QStringLiteral("番茄"));
    value.replace(QStringLiteral("马铃薯"), QStringLiteral("土豆"));
    value.replace(QStringLiteral("全蛋"), QStringLiteral("鸡蛋"));
    value.replace(QStringLiteral("生菜油"), QStringLiteral("植物油"));
    const QHash<QString, QString> aliases = {
        {QStringLiteral("生姜"), QStringLiteral("姜")},
        {QStringLiteral("老姜"), QStringLiteral("姜")},
        {QStringLiteral("嫩姜"), QStringLiteral("姜")},
        {QStringLiteral("黄姜"), QStringLiteral("姜")},
        {QStringLiteral("大葱"), QStringLiteral("葱")},
        {QStringLiteral("小葱"), QStringLiteral("葱")},
        {QStringLiteral("香葱"), QStringLiteral("葱")},
        {QStringLiteral("葱白"), QStringLiteral("葱")},
        {QStringLiteral("大蒜"), QStringLiteral("蒜")},
        {QStringLiteral("蒜瓣"), QStringLiteral("蒜")},
        {QStringLiteral("蒜蓉"), QStringLiteral("蒜")},
    };
    value = aliases.value(value, value);
    return value.trimmed();
}

bool ShoppingListService::isSpice(const QString &name)
{
    static const QStringList spices = {
        QStringLiteral("八角"), QStringLiteral("大料"), QStringLiteral("桂皮"),
        QStringLiteral("肉桂"), QStringLiteral("花椒"), QStringLiteral("香叶"),
        QStringLiteral("月桂"), QStringLiteral("丁香"), QStringLiteral("草果"),
        QStringLiteral("豆蔻"), QStringLiteral("陈皮"), QStringLiteral("孜然"),
        QStringLiteral("茴香"), QStringLiteral("迷迭香"), QStringLiteral("香草"),
        QStringLiteral("五香粉"), QStringLiteral("咖喱粉")};
    for (const QString &term : spices) {
        if (name.contains(term))
            return true;
    }
    return false;
}

bool ShoppingListService::isCommonPantrySeasoning(const QString &name)
{
    if (isSpice(name))
        return false;
    static const QStringList exactNonPurchases = {
        QStringLiteral("水"), QStringLiteral("清水"), QStringLiteral("凉水"),
        QStringLiteral("温水"), QStringLiteral("开水"), QStringLiteral("饮用水"),
        QStringLiteral("冰块")};
    if (exactNonPurchases.contains(name.trimmed()))
        return true;
    static const QStringList common = {
        QStringLiteral("盐"), QStringLiteral("味精"), QStringLiteral("鸡精"),
        QStringLiteral("白糖"), QStringLiteral("砂糖"), QStringLiteral("糖（"),
        QStringLiteral("酱油"), QStringLiteral("生抽"), QStringLiteral("老抽"),
        QStringLiteral("醋"), QStringLiteral("料酒"), QStringLiteral("黄酒"),
        QStringLiteral("淀粉"), QStringLiteral("植物油"), QStringLiteral("食用油"),
        QStringLiteral("色拉油"), QStringLiteral("香油"), QStringLiteral("麻油"),
        QStringLiteral("胡椒粉")};
    for (const QString &term : common) {
        if (name.contains(term))
            return true;
    }
    return false;
}

QString ShoppingListService::categoryFor(const QString &name)
{
    if (isSpice(name) || name.contains(QStringLiteral("酱"))
        || name.contains(QStringLiteral("芥末")) || name.contains(QStringLiteral("鱼露")))
        return QStringLiteral("调味与香料");
    if (name.contains(QStringLiteral("猪")) || name.contains(QStringLiteral("牛"))
        || name.contains(QStringLiteral("羊")) || name.contains(QStringLiteral("鸡"))
        || name.contains(QStringLiteral("鸭")) || name.contains(QStringLiteral("鹅"))
        || name.contains(QStringLiteral("肉")) || name.contains(QStringLiteral("肝"))
        || name.contains(QStringLiteral("蛋")))
        return QStringLiteral("肉禽蛋");
    if (name.contains(QStringLiteral("鱼")) || name.contains(QStringLiteral("虾"))
        || name.contains(QStringLiteral("蟹")) || name.contains(QStringLiteral("贝")))
        return QStringLiteral("水产海鲜");
    if (name.contains(QStringLiteral("米")) || name.contains(QStringLiteral("面"))
        || name.contains(QStringLiteral("麦")) || name.contains(QStringLiteral("粉"))
        || name.contains(QStringLiteral("馒头")) || name.contains(QStringLiteral("面包")))
        return QStringLiteral("主食谷物");
    if (name.contains(QStringLiteral("奶")) || name.contains(QStringLiteral("乳"))
        || name.contains(QStringLiteral("芝士")) || name.contains(QStringLiteral("奶酪")))
        return QStringLiteral("乳制品");
    if (name.contains(QStringLiteral("豆腐")) || name.contains(QStringLiteral("豆浆"))
        || name.contains(QStringLiteral("黄豆")) || name.contains(QStringLiteral("豆皮")))
        return QStringLiteral("豆制品");
    if (name.contains(QStringLiteral("苹果")) || name.contains(QStringLiteral("梨"))
        || name.contains(QStringLiteral("橙")) || name.contains(QStringLiteral("香蕉"))
        || name.contains(QStringLiteral("葡萄")) || name.contains(QStringLiteral("桃"))
        || name.contains(QStringLiteral("果")))
        return QStringLiteral("水果");
    return QStringLiteral("蔬菜菌菇");
}

double ShoppingListService::fridgeGrams(const QString &name, double quantity, const QString &unit)
{
    const QString normalized = unit.trimmed().toLower();
    if (normalized == QLatin1String("kg") || normalized == QStringLiteral("千克"))
        return quantity * 1000.0;
    if (normalized == QStringLiteral("斤"))
        return quantity * 500.0;
    if (normalized == QLatin1String("g") || normalized == QStringLiteral("克")
        || normalized == QLatin1String("ml") || normalized == QStringLiteral("毫升"))
        return quantity;
    if (normalized == QStringLiteral("个") || normalized == QStringLiteral("只")
        || normalized == QStringLiteral("枚")) {
        double each = 100.0;
        if (name.contains(QStringLiteral("蛋"))) each = 50.0;
        else if (name.contains(QStringLiteral("番茄")) || name.contains(QStringLiteral("西红柿"))) each = 150.0;
        else if (name.contains(QStringLiteral("土豆"))) each = 180.0;
        else if (name.contains(QStringLiteral("苹果")) || name.contains(QStringLiteral("梨"))) each = 200.0;
        return quantity * each;
    }
    return quantity * 100.0; // “份”及未标单位按一份约100g折算。
}

QList<ShoppingListItem> ShoppingListService::build(int userId, const RecommendResult &plan,
                                                   int planDays) const
{
    QList<ShoppingListItem> result;
    if (userId <= 0 || !plan.valid)
        return result;
    planDays = qBound(1, planDays, 7);

    QHash<QString, Aggregate> aggregated;
    const QList<MealSlot> mealSlots = {plan.breakfast, plan.lunch, plan.dinner};
    for (const MealSlot &slot : mealSlots) {
        for (const Recipe &summary : slot.dishes) {
            const Recipe recipe = RecipeDAO().findById(summary.id);
            for (const RecipeIngredient &ingredient : recipe.ingredients) {
                const QString canonical = normalizedIngredientName(ingredient.foodName);
                if (canonical.isEmpty() || isCommonPantrySeasoning(canonical))
                    continue;
                Aggregate &entry = aggregated[canonical];
                if (entry.displayName.isEmpty())
                    entry.displayName = canonical;
                entry.planned += qMax(0.0, ingredient.quantity) * planDays;
            }
        }
    }

    const QList<FridgeItem> inventory = FridgeDAO().listByUser(userId);
    for (const FridgeItem &item : inventory) {
        const QString pantryName = normalizedIngredientName(item.foodName);
        auto exact = aggregated.find(pantryName);
        if (exact != aggregated.end()) {
            exact->fridge += fridgeGrams(item.foodName, item.quantity, item.unit);
            continue;
        }
        for (auto it = aggregated.begin(); it != aggregated.end(); ++it) {
            if (nearName(it.key(), pantryName)) {
                it->fridge += fridgeGrams(item.foodName, item.quantity, item.unit);
                break;
            }
        }
    }

    for (auto it = aggregated.cbegin(); it != aggregated.cend(); ++it) {
        const double buy = qMax(0.0, it->planned - it->fridge);
        if (buy < 0.5)
            continue;
        ShoppingListItem item;
        item.name = it->displayName;
        item.category = categoryFor(it.key());
        item.plannedGrams = it->planned;
        item.fridgeGrams = qMin(it->planned, it->fridge);
        item.buyGrams = buy;
        item.spice = isSpice(it.key());
        result.append(item);
    }
    std::sort(result.begin(), result.end(), [](const ShoppingListItem &a, const ShoppingListItem &b) {
        if (a.category != b.category)
            return a.category < b.category;
        return a.name < b.name;
    });
    return result;
}

QString ShoppingListService::toShareText(const QList<ShoppingListItem> &items,
                                         const QString &scopeLabel)
{
    QString text = QStringLiteral("膳衡智能购物清单（%1）\n生成时间：%2\n")
                       .arg(scopeLabel, QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
    QString category;
    for (const ShoppingListItem &item : items) {
        if (category != item.category) {
            category = item.category;
            text += QStringLiteral("\n【%1】\n").arg(category);
        }
        text += QStringLiteral("□ %1  %2").arg(item.name, formatGrams(item.buyGrams));
        if (item.fridgeGrams > 0.0)
            text += QStringLiteral("（已扣除冰箱 %1）").arg(formatGrams(item.fridgeGrams));
        text += QLatin1Char('\n');
    }
    if (items.isEmpty())
        text += QStringLiteral("\n当前冰箱库存已能覆盖该方案，无需额外购买。\n");
    text += QStringLiteral("\n注：常备盐、味精、鸡精、普通酱油等未列入；八角、桂皮等香料会保留。\n");
    return text;
}
