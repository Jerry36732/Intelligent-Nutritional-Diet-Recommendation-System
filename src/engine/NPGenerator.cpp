#include "NPGenerator.h"
#include "ScoreCalculator.h"
#include "../services/UserService.h"

#include <QRandomGenerator>
#include <QSet>
#include <QVector>
#include <QtMath>
#include <cmath>
#include <algorithm>

namespace {
double proteinPerKg(const QString &goal)
{
    const QString g = goal.toLower();
    if (g == QLatin1String("gain"))
        return 1.8;
    if (g == QLatin1String("lose"))
        return 1.2;
    return 1.5;
}
} // namespace

NPGenerator::NPGenerator(const User &user, const QList<ScoredRecipe> &candidates)
    : m_user(user)
    , m_candidates(candidates)
{
    UserService svc;
    m_dailyCal = user.calorieTarget > 0 ? user.calorieTarget : svc.calculateDailyCalories(user);
    m_dailyProtein = user.weight > 0 ? user.weight * proteinPerKg(user.goal) : 100.0;
}

Recipe NPGenerator::pickFromRole(const QString &role,
                                 double targetCal,
                                 const QSet<int> &used,
                                 bool allowReuseStaple) const
{
    struct Item {
        ScoredRecipe s;
        double fit = -1e12;
    };
    QVector<Item> pool;
    const double lo = targetCal * 0.45;
    const double hi = targetCal * 1.65;

    for (const ScoredRecipe &s : m_candidates) {
        if (!s.recipe.isValid())
            continue;
        if (!m_diversity.allows(s.recipe, used, allowReuseStaple))
            continue;
        if (!role.isEmpty() && s.recipe.dishRole != role && role != QLatin1String("any"))
            continue;
        if (targetCal > 0 && (s.recipe.totalCalories < lo || s.recipe.totalCalories > hi))
            continue;

        Item it;
        it.s = s;
        it.fit = s.totalScore()
                 + ScoreCalculator::evaluate(s.recipe, qMax(1.0, targetCal), m_dailyProtein * 0.2);
        // 白米饭作为主食优先
        if (role == QLatin1String("staple") && s.recipe.name == QStringLiteral("白米饭"))
            it.fit += 30.0;
        pool.append(it);
    }

    if (pool.isEmpty()) {
        for (const ScoredRecipe &s : m_candidates) {
            if (!s.recipe.isValid())
                continue;
            if (!m_diversity.allows(s.recipe, used, allowReuseStaple))
                continue;
            if (!role.isEmpty() && s.recipe.dishRole != role && role != QLatin1String("any"))
                continue;
            Item it;
            it.s = s;
            it.fit = s.totalScore();
            pool.append(it);
        }
    }

    // 多样性过严导致空池时降级：仅避开当日已用
    if (pool.isEmpty()) {
        for (const ScoredRecipe &s : m_candidates) {
            if (!s.recipe.isValid())
                continue;
            if (!allowReuseStaple && used.contains(s.recipe.id))
                continue;
            if (!role.isEmpty() && s.recipe.dishRole != role && role != QLatin1String("any"))
                continue;
            Item it;
            it.s = s;
            it.fit = s.totalScore();
            pool.append(it);
        }
    }

    if (pool.isEmpty())
        return Recipe{};

    std::sort(pool.begin(), pool.end(), [](const Item &a, const Item &b) { return a.fit > b.fit; });
    const int topN = qMin(5, pool.size());
    return pool[QRandomGenerator::global()->bounded(topN)].s.recipe;
}

MealSlot NPGenerator::composeBreakfast(double targetCal, QSet<int> &used) const
{
    MealSlot slot;
    slot.mealLabel = QStringLiteral("早餐");
    Recipe main = pickFromRole(QStringLiteral("breakfast"), targetCal * 0.82, used);
    if (!main.isValid())
        main = pickFromRole(QStringLiteral("any"), targetCal * 0.82, used);
    if (main.isValid()) {
        used.insert(main.id);
        slot.dishes.append(main);
    }
    Recipe drink = pickFromRole(QStringLiteral("drink"), targetCal * 0.18, used);
    if (drink.isValid()) {
        used.insert(drink.id);
        slot.dishes.append(drink);
    }
    return slot;
}

MealSlot NPGenerator::composeMainMeal(const QString &label, double targetCal, QSet<int> &used) const
{
    MealSlot slot;
    slot.mealLabel = label;

    Recipe meat = pickFromRole(QStringLiteral("meat"), targetCal * 0.40, used);
    if (meat.isValid()) {
        used.insert(meat.id);
        slot.dishes.append(meat);
    }
    Recipe veg = pickFromRole(QStringLiteral("vegetable"), targetCal * 0.22, used);
    if (veg.isValid()) {
        used.insert(veg.id);
        slot.dishes.append(veg);
    }
    Recipe staple = pickFromRole(QStringLiteral("staple"), targetCal * 0.28, used, true);
    if (staple.isValid())
        slot.dishes.append(staple);

    // 午、晚餐必须完整地含有一荤、一素和且仅有一份主食；候选不足时让本轮失败，
    // 由生成器换一组候选，而不是用任意菜品补位造成重复主食或缺少荤素搭配。
    if (!meat.isValid() || !veg.isValid() || !staple.isValid())
        return MealSlot{};

    if (QRandomGenerator::global()->bounded(100) < 40) {
        Recipe soup = pickFromRole(QStringLiteral("soup"), targetCal * 0.10, used);
        if (soup.isValid()) {
            used.insert(soup.id);
            slot.dishes.append(soup);
        }
    }

    return slot;
}

RecommendResult NPGenerator::generateDailyPlan(const RecommendResult *previous) const
{
    RecommendResult best;
    best.valid = false;
    double bestFit = -1.0;

    QSet<int> hardExclude;
    if (previous && previous->valid) {
        for (int id : previous->breakfast.ids())
            hardExclude.insert(id);
        for (int id : previous->lunch.ids())
            hardExclude.insert(id);
        for (int id : previous->dinner.ids())
            hardExclude.insert(id);
    }

    const double bCal = m_dailyCal * 0.30;
    const double lCal = m_dailyCal * 0.40;
    const double dCal = m_dailyCal * 0.30;

    QList<RecommendResult> trials;
    for (int i = 0; i < 6; ++i) {
        QSet<int> used = hardExclude;
        RecommendResult plan;
        plan.breakfast = composeBreakfast(bCal, used);
        plan.lunch = composeMainMeal(QStringLiteral("午餐"), lCal, used);
        plan.dinner = composeMainMeal(QStringLiteral("晚餐"), dCal, used);
        plan.valid = plan.breakfast.isValid() && plan.lunch.isValid() && plan.dinner.isValid();
        if (!plan.valid && !hardExclude.isEmpty()) {
            used.clear();
            plan.breakfast = composeBreakfast(bCal, used);
            plan.lunch = composeMainMeal(QStringLiteral("午餐"), lCal, used);
            plan.dinner = composeMainMeal(QStringLiteral("晚餐"), dCal, used);
            plan.valid = plan.breakfast.isValid() && plan.lunch.isValid() && plan.dinner.isValid();
        }
        if (plan.valid) {
            if (m_diversity.isEnabled() && !m_diversity.planLooksDiverse(plan) && i < 5)
                continue; // 多样性不足则继续尝试
            trials.append(plan);
        }
    }

    // 若全被多样性滤掉，放宽再试一轮
    if (trials.isEmpty()) {
        for (int i = 0; i < 4; ++i) {
            QSet<int> used = hardExclude;
            RecommendResult plan;
            plan.breakfast = composeBreakfast(bCal, used);
            plan.lunch = composeMainMeal(QStringLiteral("午餐"), lCal, used);
            plan.dinner = composeMainMeal(QStringLiteral("晚餐"), dCal, used);
            plan.valid = plan.breakfast.isValid() && plan.lunch.isValid() && plan.dinner.isValid();
            if (plan.valid)
                trials.append(plan);
        }
    }

    if (trials.isEmpty())
        return best;

    best = optimizePlan(trials);
    if (!best.valid)
        return best;
    const double fit = calculateFitness(best);
    const double totalCal = best.breakfast.totalCalories() + best.lunch.totalCalories()
                            + best.dinner.totalCalories();
    const double totalProtein = best.breakfast.totalProtein() + best.lunch.totalProtein()
                                + best.dinner.totalProtein();

    best.summary = QStringLiteral(
                       "【RDSS+NP】日目标 %1 kcal / 蛋白约 %2 g。"
                       "早餐「%3」；午餐「%4」；晚餐「%5」。"
                       "合计约 %6 kcal、蛋白 %7 g；方案适配度 %8。")
                       .arg(m_dailyCal)
                       .arg(m_dailyProtein, 0, 'f', 0)
                       .arg(best.breakfast.title())
                       .arg(best.lunch.title())
                       .arg(best.dinner.title())
                       .arg(static_cast<int>(qRound(totalCal)))
                       .arg(totalProtein, 0, 'f', 1)
                       .arg(fit, 0, 'f', 3);
    best.valid = true;
    Q_UNUSED(bestFit);
    return best;
}

double NPGenerator::calculateFitness(const RecommendResult &plan) const
{
    if (!plan.valid)
        return 0.0;

    const double totalCal = plan.breakfast.totalCalories() + plan.lunch.totalCalories()
                            + plan.dinner.totalCalories();
    const double totalP = plan.breakfast.totalProtein() + plan.lunch.totalProtein()
                          + plan.dinner.totalProtein();
    const double totalC = plan.breakfast.totalCarbs() + plan.lunch.totalCarbs()
                          + plan.dinner.totalCarbs();
    const double totalF = plan.breakfast.totalFat() + plan.lunch.totalFat()
                          + plan.dinner.totalFat();

    // S_calories：接近日目标越好（高斯型）
    const double calErr = qAbs(totalCal - m_dailyCal) / qMax(1.0, double(m_dailyCal));
    const double sCal = qExp(-calErr * calErr * 4.0);

    // S_macronutrients：蛋白优先，碳水/脂肪粗略约束
    const double pErr = qAbs(totalP - m_dailyProtein) / qMax(1.0, m_dailyProtein);
    const double sMacro = qExp(-pErr * pErr * 3.0)
                          * qBound(0.55, 1.0 - qAbs(totalC / qMax(1.0, totalCal) * 4.0 - 0.5), 1.0)
                          * qBound(0.55, 1.0 - qAbs(totalF / qMax(1.0, totalCal) * 9.0 - 0.25), 1.0);

    // S_micronutrients：用营养增强分代理（论文微营养适配的简化）
    double micro = 0.0;
    int n = 0;
    auto addMicro = [&](const MealSlot &slot) {
        for (const Recipe &r : slot.dishes) {
            for (const ScoredRecipe &s : m_candidates) {
                if (s.recipe.id == r.id) {
                    micro += s.nutritionBoost;
                    ++n;
                    break;
                }
            }
        }
    };
    addMicro(plan.breakfast);
    addMicro(plan.lunch);
    addMicro(plan.dinner);
    const double sMicro = n > 0 ? qBound(0.4, 0.6 + micro / (n * 20.0), 1.2) : 0.7;

    // NP_fitness = S_calories * S_macronutrients * S_micronutrients
    return sCal * sMacro * sMicro;
}

RecommendResult NPGenerator::optimizePlan(const QList<RecommendResult> &plans) const
{
    RecommendResult best;
    double bestFit = -1.0;
    for (const RecommendResult &p : plans) {
        if (!p.valid)
            continue;
        double f = calculateFitness(p);
        if (m_diversity.isEnabled() && m_diversity.planLooksDiverse(p))
            f += 0.08; // 多样性加分
        if (f > bestFit) {
            bestFit = f;
            best = p;
        }
    }
    return best;
}
