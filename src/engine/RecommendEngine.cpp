#include "RecommendEngine.h"
#include "DiversityFilter.h"
#include "KnowledgeBase.h"
#include "NPGenerator.h"
#include "RDSSEngine.h"
#include "ScoreCalculator.h"
#include "../dao/RecipeDAO.h"
#include "../dao/RecommendationHistoryDAO.h"
#include "../services/UserService.h"

#include <QRandomGenerator>
#include <QVector>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace {
QList<Recipe> buildDrinkCandidates(RecipeDAO &dao)
{
    QList<Recipe> drinks = dao.findByRole(QStringLiteral("drink"));
    QSet<int> ids;
    for (const Recipe &r : drinks)
        ids.insert(r.id);

    const QStringList keywords = {
        QStringLiteral("汁"), QStringLiteral("牛奶"), QStringLiteral("豆浆"),
        QStringLiteral("酸奶"), QStringLiteral("拿铁"), QStringLiteral("咖啡"),
        QStringLiteral("奶茶"), QStringLiteral("米浆"), QStringLiteral("椰汁"),
        QStringLiteral("梨汁"), QStringLiteral("苹果汁"), QStringLiteral("橙汁"),
        QStringLiteral("柠檬"), QStringLiteral("饮"),
    };
    for (const Recipe &r : dao.findAll()) {
        if (ids.contains(r.id))
            continue;
        if (r.dishRole == QLatin1String("drink")) {
            drinks.append(r);
            ids.insert(r.id);
            continue;
        }
        for (const QString &kw : keywords) {
            if (r.name.contains(kw)) {
                drinks.append(r);
                ids.insert(r.id);
                break;
            }
        }
    }
    return drinks;
}
} // namespace

double RecommendEngine::proteinPerKg(const QString &goal)
{
    const QString g = goal.toLower();
    if (g == QLatin1String("gain"))
        return 1.8;
    if (g == QLatin1String("lose"))
        return 1.2;
    return 1.5;
}

QStringList RecommendEngine::splitKeywords(const QString &text)
{
    QString normalized = text;
    normalized.replace(QLatin1Char('，'), QLatin1Char(','));
    normalized.replace(QLatin1Char('、'), QLatin1Char(','));
    normalized.replace(QLatin1Char(';'), QLatin1Char(','));
    normalized.replace(QStringLiteral(" "), QString());
    QStringList parts = normalized.split(QLatin1Char(','), Qt::SkipEmptyParts);
    QStringList out;
    for (QString p : parts) {
        p = p.trimmed();
        if (!p.isEmpty())
            out.append(p);
    }
    return out;
}

bool RecommendEngine::hitsAllergen(const Recipe &recipe, const QStringList &allergens)
{
    if (allergens.isEmpty())
        return false;
    // 使用 RDSS 展开关键词（豆制品→豆腐等）
    User tmp;
    tmp.allergens = allergens.join(QStringLiteral("、"));
    tmp.allergies = allergens;
    RDSSEngine rdss(tmp);
    return RDSSEngine::containsAny(RDSSEngine::recipeBlob(recipe), rdss.expandedAvoidKeywords());
}

double RecommendEngine::preferenceBonus(const Recipe &recipe, const QStringList &prefs)
{
    if (prefs.isEmpty())
        return 0.0;
    double bonus = 0.0;
    for (const QString &p : prefs) {
        if (!p.isEmpty() && recipe.name.contains(p, Qt::CaseInsensitive))
            bonus += 8.0;
    }
    return bonus;
}

Recipe RecommendEngine::pickOne(const QList<Recipe> &candidates,
                                double targetCal,
                                double targetProtein,
                                const QSet<int> &excludeIds,
                                const QStringList &allergens,
                                const QStringList &prefs,
                                double *outScore) const
{
    struct Scored {
        Recipe recipe;
        double score = -1e12;
    };
    QVector<Scored> pool;
    pool.reserve(candidates.size());
    const double lo = targetCal * 0.55;
    const double hi = targetCal * 1.55;

    for (const Recipe &r : candidates) {
        if (!r.isValid() || excludeIds.contains(r.id))
            continue;
        if (hitsAllergen(r, allergens))
            continue;
        if (targetCal > 0 && (r.totalCalories < lo || r.totalCalories > hi))
            continue;
        Scored item;
        item.recipe = r;
        item.score = ScoreCalculator::evaluate(r, qMax(1.0, targetCal), targetProtein)
                     + preferenceBonus(r, prefs);
        pool.append(item);
    }

    if (pool.isEmpty()) {
        for (const Recipe &r : candidates) {
            if (!r.isValid() || excludeIds.contains(r.id) || hitsAllergen(r, allergens))
                continue;
            Scored item;
            item.recipe = r;
            item.score = ScoreCalculator::evaluate(r, qMax(1.0, targetCal), targetProtein)
                         + preferenceBonus(r, prefs);
            pool.append(item);
        }
    }

    if (pool.isEmpty()) {
        if (outScore)
            *outScore = 0.0;
        return Recipe{};
    }

    std::sort(pool.begin(), pool.end(), [](const Scored &a, const Scored &b) {
        return a.score > b.score;
    });
    const int topN = qMin(6, pool.size());
    const int pick = QRandomGenerator::global()->bounded(topN);
    if (outScore)
        *outScore = pool[pick].score;
    return pool[pick].recipe;
}

MealSlot RecommendEngine::composeBreakfast(const QList<Recipe> &pool,
                                           const QList<Recipe> &drinks,
                                           double targetCal,
                                           double targetProtein,
                                           QSet<int> &used,
                                           const QStringList &allergens,
                                           const QStringList &prefs) const
{
    MealSlot slot;
    slot.mealLabel = QStringLiteral("早餐");

    QList<Recipe> mains;
    for (const Recipe &r : pool) {
        if (r.dishRole != QLatin1String("drink"))
            mains.append(r);
    }
    if (mains.isEmpty())
        mains = pool;

    const double mainCal = targetCal * 0.82;
    const double drinkCal = targetCal * 0.18;
    double score = 0.0;

    Recipe dish = pickOne(mains, mainCal, targetProtein * 0.85, used, allergens, prefs, &score);
    if (dish.isValid()) {
        used.insert(dish.id);
        slot.dishes.append(dish);
    }

    Recipe drink = pickOne(drinks, drinkCal, targetProtein * 0.08, used, allergens, prefs, &score);
    if (drink.isValid()) {
        used.insert(drink.id);
        slot.dishes.append(drink);
    }

    return slot;
}

MealSlot RecommendEngine::composeMulti(const QString &label,
                                       const QList<Recipe> &meats,
                                       const QList<Recipe> &vegs,
                                       const QList<Recipe> &staples,
                                       const QList<Recipe> &soups,
                                       double targetCal,
                                       double targetProtein,
                                       QSet<int> &used,
                                       const QStringList &allergens,
                                       const QStringList &prefs,
                                       bool includeStaple) const
{
    MealSlot slot;
    slot.mealLabel = label;

    // 荤素搭配：主荤约 45%，素菜约 25%，主食约 25%，汤约 15%（可选）
    // 午餐/晚餐结构：一荤、一素、一份主食（优先白米饭），汤为可选项。
    const double meatCal = targetCal * 0.40;
    const double vegCal = targetCal * 0.22;
    const double stapleCal = targetCal * 0.28;
    const double soupCal = targetCal * 0.10;
    const double meatProtein = targetProtein * 0.55;
    const double vegProtein = targetProtein * 0.15;
    const double stapleProtein = targetProtein * 0.20;

    double score = 0.0;
    Recipe meat = pickOne(meats, meatCal, meatProtein, used, allergens, prefs, &score);
    if (meat.isValid()) {
        used.insert(meat.id);
        slot.dishes.append(meat);
    }

    Recipe veg = pickOne(vegs, vegCal, vegProtein, used, allergens, prefs, &score);
    if (veg.isValid()) {
        used.insert(veg.id);
        slot.dishes.append(veg);
    }

    if (includeStaple) {
        // 主食允许午餐、晚餐重复使用同一标准白米饭，不受跨餐去重影响。
        Recipe staple = pickOne(staples, stapleCal, stapleProtein, {}, allergens, prefs, &score);
        if (staple.isValid()) {
            slot.dishes.append(staple);
        }
    }

    // 晚餐额外尝试加汤，增强「多道菜」感
    if (QRandomGenerator::global()->bounded(100) < 45) {
        Recipe soup = pickOne(soups, soupCal, targetProtein * 0.1, used, allergens, prefs, &score);
        if (soup.isValid()) {
            used.insert(soup.id);
            slot.dishes.append(soup);
        }
    }

    // 兜底：若荤素都空，从 meats+vegs+staples 混选 2 道
    if (slot.dishes.size() < 2) {
        // 不从主食池补位，确保每餐有且仅有一道主食。
        QList<Recipe> fallback = meats + vegs + soups;
        while (slot.dishes.size() < 2) {
            Recipe extra = pickOne(fallback, targetCal / 2.0, targetProtein / 2.0, used, allergens, prefs, &score);
            if (!extra.isValid())
                break;
            used.insert(extra.id);
            slot.dishes.append(extra);
        }
    }

    return slot;
}

RecommendResult RecommendEngine::generatePlan(const User &user,
                                              const RecommendResult *previous) const
{
    RecipeDAO dao;
    QList<Recipe> all = dao.findAll();
    // 补充分角色池，保证 NP 有足够候选
    auto appendUnique = [&](const QList<Recipe> &extra) {
        QSet<int> ids;
        for (const Recipe &r : all)
            ids.insert(r.id);
        for (const Recipe &r : extra) {
            if (!ids.contains(r.id)) {
                all.append(r);
                ids.insert(r.id);
            }
        }
    };
    appendUnique(dao.findByRole(QStringLiteral("breakfast")));
    appendUnique(dao.findByRole(QStringLiteral("meat")));
    appendUnique(dao.findByRole(QStringLiteral("vegetable")));
    appendUnique(dao.findByRole(QStringLiteral("staple")));
    appendUnique(dao.findByRole(QStringLiteral("soup")));
    appendUnique(dao.findByRole(QStringLiteral("drink")));

    RDSSEngine rdss(user);
    QList<ScoredRecipe> scored = rdss.process(all);

    // 候选太少：降级旧规则（仍带展开过敏过滤）
    if (scored.size() < 12) {
        RecommendResult legacy = generateLegacyPlan(user, previous);
        if (legacy.valid) {
            legacy.summary = QStringLiteral("【兜底规则】") + legacy.summary;
            return legacy;
        }
    }

    NPGenerator np(user, scored);
    DiversityFilter diversity(true);
    if (user.id > 0) {
        RecommendationHistoryDAO hist;
        diversity.setRecentRecipeIds(hist.recentRecipeIds(user.id, 3));
    }
    np.setDiversityFilter(diversity);

    RecommendResult plan = np.generateDailyPlan(previous);

    // 重新生成时至少换两餐
    if (previous && previous->valid && plan.valid) {
        auto changed = [&](const MealSlot &a, const MealSlot &b) {
            return a.title() != b.title();
        };
        int diffs = 0;
        if (changed(plan.breakfast, previous->breakfast))
            ++diffs;
        if (changed(plan.lunch, previous->lunch))
            ++diffs;
        if (changed(plan.dinner, previous->dinner))
            ++diffs;
        for (int attempt = 0; attempt < 6 && diffs < 2; ++attempt) {
            RecommendResult trial = np.generateDailyPlan(previous);
            if (!trial.valid)
                continue;
            int d = 0;
            if (changed(trial.breakfast, previous->breakfast))
                ++d;
            if (changed(trial.lunch, previous->lunch))
                ++d;
            if (changed(trial.dinner, previous->dinner))
                ++d;
            if (d > diffs) {
                plan = trial;
                diffs = d;
            }
        }
    }

    if (!plan.valid)
        return generateLegacyPlan(user, previous);

    KnowledgeBase kb;
    plan.reasons = kb.buildRecommendationReasons(user, plan);
    if (!plan.reasons.isEmpty()) {
        plan.summary += QStringLiteral("\n推荐理由：") + plan.reasons.join(QStringLiteral("；"));
    }

    if (user.id > 0) {
        RecommendationHistoryDAO hist;
        hist.recordPlan(user.id, plan);
    }
    return plan;
}

RecommendResult RecommendEngine::generateLegacyPlan(const User &user,
                                                    const RecommendResult *previous) const
{
    RecommendResult result;
    result.valid = false;

    UserService userService;
    int dailyCal = user.calorieTarget;
    if (dailyCal <= 0)
        dailyCal = userService.calculateDailyCalories(user);

    const double breakfastCal = dailyCal * 0.30;
    const double lunchCal = dailyCal * 0.40;
    const double dinnerCal = dailyCal * 0.30;

    const double dailyProtein = user.weight * proteinPerKg(user.goal);
    const double breakfastProtein = dailyProtein * 0.30;
    const double lunchProtein = dailyProtein * 0.40;
    const double dinnerProtein = dailyProtein * 0.30;

    const QStringList allergens = RDSSEngine(user).expandedAvoidKeywords();
    QStringList prefs = splitKeywords(user.preferences);
    prefs += user.dietaryChoices;
    prefs += user.nutritionalDeficiencies;
    prefs.removeDuplicates();

    RecipeDAO dao;
    QList<Recipe> breakfastPool = dao.findByRole(QStringLiteral("breakfast"));
    if (breakfastPool.size() < 5)
        breakfastPool += dao.findByCategory(QStringLiteral("早餐"));
    QList<Recipe> meats = dao.findByRole(QStringLiteral("meat"));
    QList<Recipe> vegs = dao.findByRole(QStringLiteral("vegetable"));
    QList<Recipe> staples = dao.findByRole(QStringLiteral("staple"));
    QList<Recipe> soups = dao.findByRole(QStringLiteral("soup"));
    QList<Recipe> mixed = dao.findByRole(QStringLiteral("mixed"));

    // 素菜池不足时，从全部菜谱里按菜名启发式补充
    if (vegs.size() < 10) {
        for (const Recipe &r : dao.findAll()) {
            const QString n = r.name;
            if (n.contains(QStringLiteral("拌")) || n.contains(QStringLiteral("青菜"))
                || n.contains(QStringLiteral("西兰花")) || n.contains(QStringLiteral("豆腐"))
                || n.contains(QStringLiteral("瓜")) || n.contains(QStringLiteral("菇"))
                || n.contains(QStringLiteral("茄")) || n.contains(QStringLiteral("豆芽"))
                || n.contains(QStringLiteral("藕")) || n.contains(QStringLiteral("菠菜"))
                || n.contains(QStringLiteral("生菜")) || n.contains(QStringLiteral("白菜"))) {
                vegs.append(r);
            }
        }
    }

    if (meats.isEmpty())
        meats = dao.findByCategory(QStringLiteral("午餐")) + mixed;
    if (vegs.isEmpty())
        vegs = dao.findByCategory(QStringLiteral("晚餐")) + mixed;
    if (staples.isEmpty())
        staples = mixed;
    if (soups.isEmpty())
        soups = mixed;

    const QList<Recipe> drinks = buildDrinkCandidates(dao);

    // 数据维护脚本会提供标准“白米饭”食谱；存在时固定使用它作为主食。
    QList<Recipe> whiteRice;
    for (const Recipe &recipe : staples) {
        if (recipe.name == QStringLiteral("白米饭"))
            whiteRice.append(recipe);
    }
    if (!whiteRice.isEmpty())
        staples = whiteRice;

    QSet<int> hardExclude;
    if (previous && previous->valid) {
        for (int id : previous->breakfast.ids())
            hardExclude.insert(id);
        for (int id : previous->lunch.ids())
            hardExclude.insert(id);
        for (int id : previous->dinner.ids())
            hardExclude.insert(id);
    }

    auto buildOnce = [&](const QSet<int> &seedExclude) {
        QSet<int> used = seedExclude;
        RecommendResult plan;
        plan.breakfast = composeBreakfast(breakfastPool, drinks, breakfastCal, breakfastProtein, used, allergens, prefs);
        plan.lunch = composeMulti(QStringLiteral("午餐"), meats, vegs, staples, soups,
                                  lunchCal, lunchProtein, used, allergens, prefs, true);
        plan.dinner = composeMulti(QStringLiteral("晚餐"), meats, vegs, staples, soups,
                                   dinnerCal, dinnerProtein, used, allergens, prefs, true);
        plan.valid = plan.breakfast.isValid() && plan.lunch.isValid() && plan.dinner.isValid();
        return plan;
    };

    result = buildOnce(hardExclude);
    if (!result.valid)
        result = buildOnce({});

    // 重新生成时至少替换两餐
    if (previous && previous->valid && result.valid) {
        auto changed = [&](const MealSlot &a, const MealSlot &b) {
            return a.title() != b.title();
        };
        int diffs = 0;
        if (changed(result.breakfast, previous->breakfast))
            ++diffs;
        if (changed(result.lunch, previous->lunch))
            ++diffs;
        if (changed(result.dinner, previous->dinner))
            ++diffs;

        for (int attempt = 0; attempt < 8 && diffs < 2; ++attempt) {
            RecommendResult trial = buildOnce(hardExclude);
            if (!trial.valid)
                continue;
            int d = 0;
            if (changed(trial.breakfast, previous->breakfast))
                ++d;
            if (changed(trial.lunch, previous->lunch))
                ++d;
            if (changed(trial.dinner, previous->dinner))
                ++d;
            if (d > diffs) {
                result = trial;
                diffs = d;
            }
        }
    }

    if (!result.valid) {
        result.summary = QStringLiteral("推荐失败：可用菜谱不足，请先导入更多食谱。");
        return result;
    }

    const double totalCal = result.breakfast.totalCalories()
                            + result.lunch.totalCalories()
                            + result.dinner.totalCalories();
    const double totalProtein = result.breakfast.totalProtein()
                                + result.lunch.totalProtein()
                                + result.dinner.totalProtein();

    result.summary = QStringLiteral(
        "日目标 %1 kcal / 蛋白约 %2 g。"
        "早餐「%3」；午餐「%4」；晚餐「%5」。"
        "午/晚餐按荤素+主食（或汤）搭配；合计约 %6 kcal、蛋白 %7 g。")
        .arg(dailyCal)
        .arg(dailyProtein, 0, 'f', 0)
        .arg(result.breakfast.title())
        .arg(result.lunch.title())
        .arg(result.dinner.title())
        .arg(static_cast<int>(qRound(totalCal)))
        .arg(totalProtein, 0, 'f', 1);

    result.valid = true;
    return result;
}
