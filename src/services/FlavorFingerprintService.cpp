#include "FlavorFingerprintService.h"

#include "../dao/DatabaseManager.h"

#include <algorithm>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtMath>

namespace {
const QString kCurrentFlavorRuleSource = QStringLiteral("rule-v3-texture-baseline");

bool containsAny(const QString &text, const QStringList &keywords)
{
    for (const QString &keyword : keywords) {
        if (text.contains(keyword, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

double ingredientStrength(double grams)
{
    return qBound(4.0, 4.0 + qSqrt(qMax(0.0, grams)) * 1.45, 27.0);
}

void add(double *value, double amount)
{
    *value = qBound(0.0, *value + amount, 100.0);
}

QString changeText(const QString &label, double delta)
{
    if (qAbs(delta) < 3.0)
        return label + QStringLiteral("基本不变");
    return QStringLiteral("%1 %2%3%")
        .arg(label, delta > 0.0 ? QStringLiteral("+") : QString())
        .arg(qRound(delta));
}
}

double FlavorFingerprint::value(int index) const
{
    switch (index) {
    case 0: return sweet;
    case 1: return sour;
    case 2: return salty;
    case 3: return spicy;
    case 4: return umami;
    case 5: return aroma;
    case 6: return crispy;
    case 7: return soft;
    default: return 0.0;
    }
}

void FlavorFingerprint::setValue(int index, double newValue)
{
    newValue = qBound(0.0, newValue, 100.0);
    switch (index) {
    case 0: sweet = newValue; break;
    case 1: sour = newValue; break;
    case 2: salty = newValue; break;
    case 3: spicy = newValue; break;
    case 4: umami = newValue; break;
    case 5: aroma = newValue; break;
    case 6: crispy = newValue; break;
    case 7: soft = newValue; break;
    default: break;
    }
}

QStringList FlavorFingerprint::labels()
{
    return {QStringLiteral("甜"), QStringLiteral("酸"), QStringLiteral("咸"),
            QStringLiteral("辣"), QStringLiteral("鲜"), QStringLiteral("香"),
            QStringLiteral("酥脆"), QStringLiteral("软糯")};
}

FlavorFingerprint FlavorFingerprintService::forRecipe(const Recipe &recipe) const
{
    QSqlDatabase db = DatabaseManager::getInstance().database();
    if (recipe.id > 0 && db.isOpen()) {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT sweet,sour,salty,spicy,umami,aroma,crispy,soft,source "
            "FROM recipe_flavor_fingerprints WHERE recipe_id=:recipe"));
        query.bindValue(QStringLiteral(":recipe"), recipe.id);
        if (query.exec() && query.next()) {
            FlavorFingerprint cached;
            for (int i = 0; i < 8; ++i)
                cached.setValue(i, query.value(i).toDouble());
            // 旧版规则为所有菜品注入固定基准分。仅复用当前规则生成的缓存，
            // 其余记录会根据实际原料和步骤重新估算并覆盖。
            if (query.value(8).toString().startsWith(kCurrentFlavorRuleSource))
                return cached;
        }
    }

    QList<QPair<QString, double>> ingredients;
    for (const RecipeIngredient &ingredient : recipe.ingredients)
        ingredients.append({ingredient.foodName, ingredient.quantity});
    const FlavorFingerprint fingerprint = estimate(recipe.name, ingredients, recipe.steps,
                                                    recipe.totalFat, recipe.totalWeight);
    if (recipe.id > 0)
        persist(recipe.id, fingerprint);
    return fingerprint;
}

FlavorFingerprint FlavorFingerprintService::estimate(
    const QString &name, const QList<QPair<QString, double>> &ingredients,
    const QString &steps, double totalFat, double totalWeight) const
{
    FlavorFingerprint result;
    // 酥脆、软糯属于连续口感而不是调味。为避免普通菜品在雷达图上
    // 完全塌到中心，保留很低的中性底值；明显口感仍由原料与步骤拉开差异。
    result.crispy = 4.0;
    result.soft = 4.0;

    const QString combined = name + QLatin1Char(' ') + steps;
    const QStringList sweet = {QStringLiteral("糖"), QStringLiteral("蜂蜜"),
                               QStringLiteral("糖浆"), QStringLiteral("炼乳"),
                               QStringLiteral("果酱"), QStringLiteral("红枣"),
                               QStringLiteral("甜")};
    const QStringList sour = {QStringLiteral("醋"), QStringLiteral("柠檬"),
                              QStringLiteral("酸梅"), QStringLiteral("酸菜"),
                              QStringLiteral("番茄"), QStringLiteral("酸")};
    const QStringList salty = {QStringLiteral("盐"), QStringLiteral("酱油"),
                               QStringLiteral("豆瓣酱"), QStringLiteral("蚝油"),
                               QStringLiteral("味噌"), QStringLiteral("咸")};
    const QStringList spicy = {QStringLiteral("辣椒"), QStringLiteral("花椒"),
                               QStringLiteral("胡椒"), QStringLiteral("芥末"),
                               QStringLiteral("辣")};
    const QStringList umami = {QStringLiteral("味精"), QStringLiteral("鸡精"),
                               QStringLiteral("菌"), QStringLiteral("菇"),
                               QStringLiteral("虾"), QStringLiteral("贝"),
                               QStringLiteral("鱼"), QStringLiteral("肉"),
                               QStringLiteral("高汤"), QStringLiteral("鲜")};
    const QStringList aroma = {QStringLiteral("香油"), QStringLiteral("芝麻油"),
                               QStringLiteral("葱"), QStringLiteral("姜"),
                               QStringLiteral("蒜"), QStringLiteral("八角"),
                               QStringLiteral("桂皮"), QStringLiteral("香")};

    for (const auto &ingredient : ingredients) {
        const QString text = ingredient.first;
        const double strength = ingredientStrength(ingredient.second);
        if (containsAny(text, sweet)) add(&result.sweet, strength);
        if (containsAny(text, sour)) add(&result.sour, strength);
        if (containsAny(text, salty)) add(&result.salty, strength * 0.85);
        if (containsAny(text, spicy)) add(&result.spicy, strength);
        if (containsAny(text, umami)) add(&result.umami, strength * 0.72);
        if (containsAny(text, aroma)) add(&result.aroma, strength * 0.72);
        if (containsAny(text, {QStringLiteral("油"), QStringLiteral("脂")}))
            add(&result.aroma, strength * 0.45);
        if (containsAny(text, {QStringLiteral("糯米"), QStringLiteral("年糕"),
                               QStringLiteral("土豆"), QStringLiteral("豆腐")}))
            add(&result.soft, strength * 0.55);
    }

    if (containsAny(combined, {QStringLiteral("炸"), QStringLiteral("煎"),
                               QStringLiteral("烤"), QStringLiteral("酥"),
                               QStringLiteral("脆")})) {
        add(&result.crispy, 30.0);
        add(&result.aroma, 13.0);
    }
    if (containsAny(combined, {QStringLiteral("炖"), QStringLiteral("焖"),
                               QStringLiteral("蒸"), QStringLiteral("煮"),
                               QStringLiteral("软烂"), QStringLiteral("软糯")}))
        add(&result.soft, 30.0);
    if (containsAny(combined, {QStringLiteral("炒香"), QStringLiteral("爆香"),
                               QStringLiteral("煸香"), QStringLiteral("炝")}))
        add(&result.aroma, 20.0);
    if (containsAny(combined, sweet)) add(&result.sweet, 14.0);
    if (containsAny(combined, sour)) add(&result.sour, 14.0);
    if (containsAny(combined, spicy)) add(&result.spicy, 14.0);

    if (totalWeight > 0.0 && totalFat > 0.0)
        add(&result.aroma, qBound(0.0, totalFat / totalWeight * 100.0, 18.0));
    return result;
}

bool FlavorFingerprintService::persist(int recipeId,
                                       const FlavorFingerprint &fingerprint,
                                       const QString &source) const
{
    QSqlDatabase db = DatabaseManager::getInstance().database();
    if (recipeId <= 0 || !db.isOpen())
        return false;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO recipe_flavor_fingerprints(recipe_id,sweet,sour,salty,spicy,umami,"
        "aroma,crispy,soft,source,updated_at) "
        "VALUES(:recipe,:sweet,:sour,:salty,:spicy,:umami,:aroma,:crispy,:soft,:source,"
        "datetime('now','localtime')) ON CONFLICT(recipe_id) DO UPDATE SET "
        "sweet=excluded.sweet,sour=excluded.sour,salty=excluded.salty,spicy=excluded.spicy,"
        "umami=excluded.umami,aroma=excluded.aroma,crispy=excluded.crispy,soft=excluded.soft,"
        "source=excluded.source,updated_at=datetime('now','localtime')"));
    query.bindValue(QStringLiteral(":recipe"), recipeId);
    for (int i = 0; i < 8; ++i)
        query.bindValue(QStringLiteral(":%1").arg(
            QStringList{QStringLiteral("sweet"), QStringLiteral("sour"),
                        QStringLiteral("salty"), QStringLiteral("spicy"),
                        QStringLiteral("umami"), QStringLiteral("aroma"),
                        QStringLiteral("crispy"), QStringLiteral("soft")}.at(i)),
                        fingerprint.value(i));
    query.bindValue(QStringLiteral(":source"), source);
    return query.exec();
}

int FlavorFingerprintService::similarity(const FlavorFingerprint &before,
                                         const FlavorFingerprint &after)
{
    double squared = 0.0;
    for (int i = 0; i < 8; ++i) {
        const double difference = before.value(i) - after.value(i);
        squared += difference * difference;
    }
    const double normalizedDistance = qSqrt(squared / 8.0);
    return qBound(0, qRound(100.0 - normalizedDistance), 100);
}

QString FlavorFingerprintService::comparisonSummary(const FlavorFingerprint &before,
                                                    const FlavorFingerprint &after)
{
    struct Change { int index; double delta; };
    QList<Change> changes;
    for (int i = 0; i < 8; ++i)
        changes.append({i, after.value(i) - before.value(i)});
    std::sort(changes.begin(), changes.end(), [](const Change &left, const Change &right) {
        return qAbs(left.delta) > qAbs(right.delta);
    });
    const QStringList labels = FlavorFingerprint::labels();
    QStringList parts;
    parts << QStringLiteral("原版风味相似度 %1%").arg(similarity(before, after));
    for (const Change &change : changes.mid(0, 3))
        parts << changeText(labels.at(change.index), change.delta);
    return parts.join(QStringLiteral(" · "));
}
