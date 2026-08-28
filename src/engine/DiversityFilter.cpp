#include "DiversityFilter.h"

#include <QSet>

DiversityFilter::DiversityFilter(bool enabled)
    : m_enabled(enabled)
{
}

QStringList DiversityFilter::foodTokens(const Recipe &recipe)
{
    // 粗粒度：用菜名中的常见食材词；白米饭允许跨餐重复
    static const QStringList keys = {
        QStringLiteral("香蕉"), QStringLiteral("苹果"), QStringLiteral("菠菜"), QStringLiteral("豆腐"),
        QStringLiteral("鸡蛋"), QStringLiteral("牛奶"), QStringLiteral("牛肉"), QStringLiteral("猪肉"),
        QStringLiteral("鸡肉"), QStringLiteral("鱼"),   QStringLiteral("虾"),   QStringLiteral("西兰花"),
        QStringLiteral("番茄"), QStringLiteral("土豆"), QStringLiteral("南瓜"), QStringLiteral("玉米"),
    };
    QStringList hit;
    for (const QString &k : keys) {
        if (recipe.name.contains(k))
            hit.append(k);
    }
    for (const auto &ing : recipe.ingredients) {
        for (const QString &k : keys) {
            if (ing.foodName.contains(k) && !hit.contains(k))
                hit.append(k);
        }
    }
    return hit;
}

bool DiversityFilter::allows(const Recipe &recipe, const QSet<int> &usedToday, bool allowStapleReuse) const
{
    if (!m_enabled || !recipe.isValid())
        return true;
    if (allowStapleReuse && (recipe.dishRole == QLatin1String("staple")
                             || recipe.name == QStringLiteral("白米饭")))
        return true;
    if (usedToday.contains(recipe.id))
        return false;
    if (m_recentIds.contains(recipe.id))
        return false;
    return true;
}

bool DiversityFilter::planLooksDiverse(const RecommendResult &plan) const
{
    if (!m_enabled || !plan.valid)
        return true;
    QSet<int> ids;
    QSet<QString> tokens;
    auto add = [&](const MealSlot &slot) {
        for (const Recipe &r : slot.dishes) {
            ids.insert(r.id);
            for (const QString &t : foodTokens(r))
                tokens.insert(t);
        }
    };
    add(plan.breakfast);
    add(plan.lunch);
    add(plan.dinner);
    // 规则3：尽量 ≥3 种不同食物关键词；若关键词过少则以不同菜数兜底
    return tokens.size() >= 3 || ids.size() >= 4;
}
