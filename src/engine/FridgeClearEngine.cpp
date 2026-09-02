#include "FridgeClearEngine.h"
#include "RDSSEngine.h"
#include "../dao/RecipeDAO.h"

#include <QSet>
#include <QtMath>
#include <algorithm>

FridgeClearEngine::FridgeClearEngine(const User &user)
    : m_user(user)
{
}

bool FridgeClearEngine::nameHits(const QString &haystack, const QString &needle)
{
    const QString a = haystack.trimmed();
    const QString b = needle.trimmed();
    if (a.isEmpty() || b.isEmpty())
        return false;
    return a.contains(b, Qt::CaseInsensitive) || b.contains(a, Qt::CaseInsensitive);
}

FridgeMatchResult FridgeClearEngine::scoreRecipe(const Recipe &recipe,
                                                 const QStringList &fridgeFoods) const
{
    FridgeMatchResult out;
    out.recipe = recipe;

    QStringList ingredientNames;
    for (const auto &ing : recipe.ingredients) {
        const QString n = ing.foodName.trimmed();
        if (!n.isEmpty())
            ingredientNames.append(n);
    }
    if (ingredientNames.isEmpty())
        ingredientNames.append(recipe.name);

    out.totalIngredients = qMax(1, ingredientNames.size());
    QSet<QString> usedFridge;

    for (const QString &ing : ingredientNames) {
        bool hit = false;
        for (const QString &f : fridgeFoods) {
            if (nameHits(ing, f) || nameHits(recipe.name, f)) {
                hit = true;
                if (!out.matchedNames.contains(f))
                    out.matchedNames.append(f);
                usedFridge.insert(f);
                break;
            }
        }
        if (!hit)
            out.missingNames.append(ing);
    }

    out.matchedCount = out.matchedNames.size();
    if (out.matchedCount <= 0) {
        out.score = -1.0;
        return out;
    }

    const double coverage = double(out.matchedCount) / double(out.totalIngredients);
    const double fridgeUse = double(usedFridge.size()) / double(qMax(1, fridgeFoods.size()));
    out.matchRatio = coverage;
    out.score = out.matchedCount * 40.0 + coverage * 35.0 + fridgeUse * 25.0
                - out.missingNames.size() * 2.0
                + qMin(12.0, recipe.totalProtein * 0.08);
    return out;
}

QList<FridgeMatchResult> FridgeClearEngine::rankRecipes(const QStringList &fridgeFoods,
                                                        int limit) const
{
    QList<FridgeMatchResult> ranked;
    QStringList foods;
    for (const QString &f : fridgeFoods) {
        const QString t = f.trimmed();
        if (!t.isEmpty() && !foods.contains(t))
            foods.append(t);
    }
    if (foods.isEmpty())
        return ranked;

    RecipeDAO dao;
    QList<Recipe> all = dao.findAll();
    QList<Recipe> candidates;
    for (Recipe r : all) {
        bool nameHit = false;
        for (const QString &f : foods) {
            if (nameHits(r.name, f)) {
                nameHit = true;
                break;
            }
        }
        r.ingredients = dao.getIngredients(r.id);
        bool ingHit = false;
        for (const auto &ing : r.ingredients) {
            for (const QString &f : foods) {
                if (nameHits(ing.foodName, f)) {
                    ingHit = true;
                    break;
                }
            }
            if (ingHit)
                break;
        }
        if (nameHit || ingHit)
            candidates.append(r);
    }

    RDSSEngine rdss(m_user);
    const QStringList avoid = rdss.expandedAvoidKeywords();
    auto hitsAvoid = [&](const Recipe &r) {
        for (const QString &k : avoid) {
            if (k.isEmpty())
                continue;
            if (r.name.contains(k))
                return true;
            for (const auto &ing : r.ingredients) {
                if (ing.foodName.contains(k))
                    return true;
            }
        }
        return false;
    };

    for (const Recipe &r : candidates) {
        if (hitsAvoid(r))
            continue;
        FridgeMatchResult m = scoreRecipe(r, foods);
        if (m.score > 0)
            ranked.append(m);
    }

    std::sort(ranked.begin(), ranked.end(), [](const FridgeMatchResult &a, const FridgeMatchResult &b) {
        if (a.matchedCount != b.matchedCount)
            return a.matchedCount > b.matchedCount;
        return a.score > b.score;
    });

    if (ranked.size() > limit)
        ranked = ranked.mid(0, limit);
    return ranked;
}

Recipe FridgeClearEngine::pickBest(const QList<FridgeMatchResult> &ranked,
                                   const QSet<int> &used,
                                   const QString &rolePrefer) const
{
    for (const FridgeMatchResult &m : ranked) {
        if (!m.recipe.isValid() || used.contains(m.recipe.id))
            continue;
        if (!rolePrefer.isEmpty() && rolePrefer != QLatin1String("any")
            && m.recipe.dishRole != rolePrefer)
            continue;
        return m.recipe;
    }
    // 指定角色时不得退化成任意菜，否则可能把荤菜误当主食。
    if (rolePrefer.isEmpty() || rolePrefer == QLatin1String("any")) {
        for (const FridgeMatchResult &m : ranked) {
            if (m.recipe.isValid() && !used.contains(m.recipe.id))
                return m.recipe;
        }
    }
    return Recipe{};
}

RecommendResult FridgeClearEngine::generateDailyPlan(const QStringList &fridgeFoods) const
{
    RecommendResult plan;
    plan.valid = false;
    const QList<FridgeMatchResult> ranked = rankRecipes(fridgeFoods, 60);
    if (ranked.size() < 2)
        return plan;

    QSet<int> used;
    auto addDish = [&](MealSlot &slot, const Recipe &r) {
        if (!r.isValid())
            return;
        slot.dishes.append(r);
        used.insert(r.id);
    };

    // 早餐：优先 breakfast / drink / any
    plan.breakfast.mealLabel = QStringLiteral("早餐");
    Recipe b = pickBest(ranked, used, QStringLiteral("breakfast"));
    if (!b.isValid())
        b = pickBest(ranked, used, QStringLiteral("any"));
    addDish(plan.breakfast, b);
    Recipe drink = pickBest(ranked, used, QStringLiteral("drink"));
    if (drink.isValid())
        addDish(plan.breakfast, drink);

    // 午餐：荤 + 素 + 主食（优先白米饭）
    plan.lunch.mealLabel = QStringLiteral("午餐");
    addDish(plan.lunch, pickBest(ranked, used, QStringLiteral("meat")));
    addDish(plan.lunch, pickBest(ranked, used, QStringLiteral("vegetable")));
    Recipe lunchStaple = pickBest(ranked, used, QStringLiteral("staple"));
    for (const FridgeMatchResult &m : ranked) {
        if (m.recipe.name == QStringLiteral("白米饭")) {
            lunchStaple = m.recipe;
            break;
        }
    }
    // 白米饭可能不在匹配列表（名字不含冰箱关键词），单独取
    if (!lunchStaple.isValid() || lunchStaple.name != QStringLiteral("白米饭")) {
        RecipeDAO dao;
        for (const Recipe &r : dao.findByRole(QStringLiteral("staple"))) {
            if (r.name == QStringLiteral("白米饭")) {
                lunchStaple = r;
                break;
            }
        }
    }
    addDish(plan.lunch, lunchStaple);

    // 晚餐：再配一荤一素，主食可复用白米饭或其它
    plan.dinner.mealLabel = QStringLiteral("晚餐");
    addDish(plan.dinner, pickBest(ranked, used, QStringLiteral("meat")));
    addDish(plan.dinner, pickBest(ranked, used, QStringLiteral("vegetable")));
    Recipe dinnerStaple = pickBest(ranked, used, QStringLiteral("staple"));
    if (!dinnerStaple.isValid()) {
        for (const FridgeMatchResult &m : ranked) {
            if (m.recipe.name == QStringLiteral("白米饭")) {
                dinnerStaple = m.recipe;
                break;
            }
        }
    }
    if (!dinnerStaple.isValid()) {
        RecipeDAO dao;
        for (const Recipe &r : dao.findByRole(QStringLiteral("staple"))) {
            if (r.name == QStringLiteral("白米饭")) {
                dinnerStaple = r;
                break;
            }
        }
    }
    addDish(plan.dinner, dinnerStaple);

    plan.valid = plan.breakfast.isValid()
                 && plan.lunch.hasBalancedMainMeal()
                 && plan.dinner.hasBalancedMainMeal();
    if (!plan.valid)
        return plan;

    QStringList usedFridge;
    for (const FridgeMatchResult &m : ranked) {
        for (const QString &n : m.matchedNames) {
            if (!usedFridge.contains(n))
                usedFridge.append(n);
        }
        if (usedFridge.size() >= fridgeFoods.size())
            break;
    }
    plan.summary = QStringLiteral("【清冰箱】优先消耗：%1。缺料可外购补齐。")
                       .arg(usedFridge.isEmpty() ? fridgeFoods.join(QStringLiteral("、"))
                                                 : usedFridge.join(QStringLiteral("、")));
    plan.reasons = {QStringLiteral("三餐已结合冰箱现有食材匹配生成。")};
    return plan;
}
