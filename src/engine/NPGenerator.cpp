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

double planCalories(const RecommendResult &plan)
{
    return plan.breakfast.totalCalories() + plan.lunch.totalCalories()
           + plan.dinner.totalCalories();
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
    const bool preferWhiteRice = m_user.preferences.contains(QStringLiteral("白米饭"))
                                 || m_user.dietaryChoices.contains(QStringLiteral("白米饭"));

    // 用户明确要求白米饭作主食时，直接固定主食
    if (role == QLatin1String("staple") && preferWhiteRice) {
        for (const ScoredRecipe &s : m_candidates) {
            if (s.recipe.isValid() && s.recipe.name == QStringLiteral("白米饭"))
                return s.recipe;
        }
    }

    struct Item {
        ScoredRecipe s;
        double fit = -1e12;
    };
    QVector<Item> pool;
    // 每一道菜先围绕其在餐次中的热量预算筛选。旧范围最高允许 165%，
    // 再叠加三道菜后很容易让午晚餐整体超标。
    const double lo = targetCal * 0.35;
    const double hi = targetCal * 1.35;

    auto appendCandidate = [&](const ScoredRecipe &s) {
        Item it;
        it.s = s;
        const double calError = qAbs(s.recipe.totalCalories - targetCal)
                                / qMax(1.0, targetCal);
        // 热量贴合是硬目标，营养缺口加分只在热量相近的候选之间生效。
        // 不再用同一个“日蛋白目标的20%”惩罚所有角色，否则素菜会因为
        // 蛋白较低而被高热量菜错误替代。
        it.fit = -(calError * 260.0) + s.nutritionBoost * 1.5 + s.baseScore * 0.05;
        return it;
    };

    for (const ScoredRecipe &s : m_candidates) {
        if (!s.recipe.isValid())
            continue;
        // 小吃不得充当主食
        if (role == QLatin1String("staple")
            && (s.recipe.dishRole == QLatin1String("snack")
                || s.recipe.name.contains(QStringLiteral("可乐饼"))))
            continue;
        if (!m_diversity.allows(s.recipe, used, allowReuseStaple))
            continue;
        if (!role.isEmpty() && s.recipe.dishRole != role && role != QLatin1String("any"))
            continue;
        if (targetCal > 0 && (s.recipe.totalCalories < lo || s.recipe.totalCalories > hi))
            continue;

        Item it = appendCandidate(s);
        // 白米饭作为主食优先；用户偏好含「白米饭」时进一步抬高
        if (role == QLatin1String("staple") && s.recipe.name == QStringLiteral("白米饭"))
            it.fit += preferWhiteRice ? 200.0 : 30.0;
        if (preferWhiteRice && s.recipe.name.contains(QStringLiteral("白米饭")))
            it.fit += 80.0;
        // 偏好关键词命中菜名
        for (const QString &p : User::splitLegacyText(m_user.preferences)) {
            if (!p.isEmpty() && s.recipe.name.contains(p))
                it.fit += 25.0;
        }
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
            pool.append(appendCandidate(s));
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
            pool.append(appendCandidate(s));
        }
    }

    if (pool.isEmpty())
        return Recipe{};

    std::sort(pool.begin(), pool.end(), [](const Item &a, const Item &b) { return a.fit > b.fit; });
    const int topN = qMin(3, pool.size());
    return pool[QRandomGenerator::global()->bounded(topN)].s.recipe;
}

MealSlot NPGenerator::composeBreakfast(double targetCal, QSet<int> &used) const
{
    MealSlot slot;
    slot.mealLabel = QStringLiteral("早餐");
    Recipe main = pickFromRole(QStringLiteral("breakfast"), targetCal * 0.82, used);
    // 松饼、蛋糕按甜品管理，允许在早餐缺少常规主食时作为早餐主项；
    // 午餐、晚餐只从 staple 角色选主食，因此不会误入午晚餐主食位。
    if (!main.isValid())
        main = pickFromRole(QStringLiteral("dessert"), targetCal * 0.82, used);
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

MealSlot NPGenerator::composeMainMeal(const QString &label,
                                      double targetCal,
                                      QSet<int> &used,
                                      bool forceWhiteRice) const
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

    Recipe staple;
    if (forceWhiteRice) {
        staple = findWhiteRice();
        if (!staple.isValid())
            staple = pickFromRole(QStringLiteral("staple"), targetCal * 0.28, used, true);
    } else {
        staple = pickFromRole(QStringLiteral("staple"), targetCal * 0.28, used, true);
    }
    if (staple.isValid())
        slot.dishes.append(staple);

    // 午、晚餐必须完整地含有一荤、一素和且仅有一份主食；候选不足时让本轮失败，
    // 由生成器换一组候选，而不是用任意菜品补位造成重复主食或缺少荤素搭配。
    if (!meat.isValid() || !veg.isValid() || !staple.isValid())
        return MealSlot{};

    return slot;
}

Recipe NPGenerator::findWhiteRice() const
{
    for (const ScoredRecipe &s : m_candidates) {
        if (s.recipe.isValid() && s.recipe.name == QStringLiteral("白米饭"))
            return s.recipe;
    }
    return Recipe{};
}

void NPGenerator::ensureLunchOrDinnerHasWhiteRice(RecommendResult &plan) const
{
    if (!plan.valid)
        return;

    auto mealHasWhiteRice = [](const MealSlot &slot) {
        for (const Recipe &r : slot.dishes) {
            if (r.name == QStringLiteral("白米饭"))
                return true;
        }
        return false;
    };

    if (mealHasWhiteRice(plan.lunch) || mealHasWhiteRice(plan.dinner))
        return;

    const Recipe rice = findWhiteRice();
    if (!rice.isValid())
        return;

    auto replaceStapleWithRice = [&](MealSlot &slot) {
        for (Recipe &r : slot.dishes) {
            if (r.dishRole == QLatin1String("staple")
                || r.name.contains(QStringLiteral("饭"))
                || r.name.contains(QStringLiteral("面"))
                || r.name.contains(QStringLiteral("饼"))
                || r.name.contains(QStringLiteral("粥"))) {
                r = rice;
                return;
            }
        }
        slot.dishes.append(rice);
    };

    // 随机选午餐或晚餐换成白米饭，保证至少一餐主食为白米饭
    if (QRandomGenerator::global()->bounded(2) == 0)
        replaceStapleWithRice(plan.lunch);
    else
        replaceStapleWithRice(plan.dinner);
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
    // 候选池充足时多采样若干组合，但只接纳日总热量不超过目标 10%、
    // 且任一餐不超过该餐预算 18% 的方案。宁可提示候选不足，也不输出
    // 3695/2100 kcal 这类营养上明显无效的方案。
    auto withinBudget = [&](const RecommendResult &plan) {
        if (!plan.valid)
            return false;
        return planCalories(plan) <= m_dailyCal * 1.10
               && plan.breakfast.totalCalories() <= bCal * 1.18
               && plan.lunch.totalCalories() <= lCal * 1.18
               && plan.dinner.totalCalories() <= dCal * 1.18;
    };
    for (int i = 0; i < 48; ++i) {
        QSet<int> used = hardExclude;
        RecommendResult plan;
        // 随机指定午餐或晚餐强制白米饭，保证至少一餐主食为白米饭
        const bool forceRiceLunch = (QRandomGenerator::global()->bounded(2) == 0);
        plan.breakfast = composeBreakfast(bCal, used);
        plan.lunch = composeMainMeal(QStringLiteral("午餐"), lCal, used, forceRiceLunch);
        plan.dinner = composeMainMeal(QStringLiteral("晚餐"), dCal, used, !forceRiceLunch);
        plan.valid = plan.breakfast.isValid()
                     && plan.lunch.hasBalancedMainMeal()
                     && plan.dinner.hasBalancedMainMeal();
        if (!plan.valid && !hardExclude.isEmpty()) {
            used.clear();
            plan.breakfast = composeBreakfast(bCal, used);
            plan.lunch = composeMainMeal(QStringLiteral("午餐"), lCal, used, forceRiceLunch);
            plan.dinner = composeMainMeal(QStringLiteral("晚餐"), dCal, used, !forceRiceLunch);
            plan.valid = plan.breakfast.isValid()
                         && plan.lunch.hasBalancedMainMeal()
                         && plan.dinner.hasBalancedMainMeal();
        }
        if (plan.valid) {
            ensureLunchOrDinnerHasWhiteRice(plan);
            if (!withinBudget(plan))
                continue;
            if (m_diversity.isEnabled() && !m_diversity.planLooksDiverse(plan) && i < 40)
                continue; // 多样性不足则继续尝试
            trials.append(plan);
        }
    }

    // 若全被多样性滤掉，放宽再试一轮
    if (trials.isEmpty()) {
        for (int i = 0; i < 24; ++i) {
            QSet<int> used = hardExclude;
            RecommendResult plan;
            const bool forceRiceLunch = (i % 2 == 0);
            plan.breakfast = composeBreakfast(bCal, used);
            plan.lunch = composeMainMeal(QStringLiteral("午餐"), lCal, used, forceRiceLunch);
            plan.dinner = composeMainMeal(QStringLiteral("晚餐"), dCal, used, !forceRiceLunch);
            plan.valid = plan.breakfast.isValid()
                         && plan.lunch.hasBalancedMainMeal()
                         && plan.dinner.hasBalancedMainMeal();
            if (plan.valid) {
                ensureLunchOrDinnerHasWhiteRice(plan);
                if (withinBudget(plan))
                    trials.append(plan);
            }
        }
    }

    if (trials.isEmpty())
        return best;

    best = optimizePlan(trials);
    if (!best.valid)
        return best;
    ensureLunchOrDinnerHasWhiteRice(best);
    if (!best.lunch.hasBalancedMainMeal() || !best.dinner.hasBalancedMainMeal()) {
        best.valid = false;
        best.summary = QStringLiteral("午餐和晚餐必须各包含一荤、一素和一份主食，请补充相应分类食谱后重试。");
        return best;
    }
    const double fit = calculateFitness(best);
    const double totalCal = planCalories(best);
    if (totalCal > m_dailyCal * 1.10) {
        best.valid = false;
        best.summary = QStringLiteral("可用菜谱无法组合出符合热量目标的完整三餐，请补充低热量菜谱后重试。");
        return best;
    }
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
    const double sCal = qExp(-calErr * calErr * 18.0);

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
        const double totalCal = planCalories(p);
        if (totalCal > m_dailyCal * 1.10)
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
