/** 验证午餐、晚餐恰好一份主食且至少包含荤菜、素菜。 */
#include "../src/engine/NPGenerator.h"

#include <QCoreApplication>
#include <QDebug>

static Recipe recipe(int id, const QString &name, const QString &role, double calories)
{
    Recipe r;
    r.id = id;
    r.name = name;
    r.dishRole = role;
    r.totalCalories = calories;
    r.totalProtein = role == QStringLiteral("meat") ? 25.0 : 5.0;
    r.totalCarbs = role == QStringLiteral("staple") ? 45.0 : 8.0;
    return r;
}

static bool balanced(const MealSlot &meal) { return meal.hasBalancedMainMeal(); }

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    User user;
    user.weight = 65;
    user.calorieTarget = 2000;

    QList<ScoredRecipe> candidates;
    int id = 1;
    for (const auto &r : {
             recipe(id++, QStringLiteral("燕麦早餐"), QStringLiteral("breakfast"), 300),
             recipe(id++, QStringLiteral("牛奶"), QStringLiteral("drink"), 80),
             recipe(id++, QStringLiteral("宫保鸡丁"), QStringLiteral("meat"), 260),
             recipe(id++, QStringLiteral("清蒸鲈鱼"), QStringLiteral("meat"), 240),
             recipe(id++, QStringLiteral("清炒西兰花"), QStringLiteral("vegetable"), 140),
             recipe(id++, QStringLiteral("蒜蓉油麦菜"), QStringLiteral("vegetable"), 130),
             recipe(id++, QStringLiteral("白米饭"), QStringLiteral("staple"), 180),
             recipe(id++, QStringLiteral("番茄蛋花汤"), QStringLiteral("soup"), 90),
         }) {
        ScoredRecipe s;
        s.recipe = r;
        s.baseScore = 50.0;
        candidates.append(s);
    }

    NPGenerator generator(user, candidates);
    const RecommendResult plan = generator.generateDailyPlan();
    const double planCalories = plan.breakfast.totalCalories() + plan.lunch.totalCalories()
                                + plan.dinner.totalCalories();
    const bool ok = plan.valid && balanced(plan.lunch) && balanced(plan.dinner)
                    && planCalories <= user.calorieTarget * 1.10;
    if (!ok) {
        qCritical() << "main meal structure invalid";
        return 1;
    }
    qInfo() << "NP main meal structure passed";

    QList<ScoredRecipe> dessertBreakfastCandidates;
    for (const ScoredRecipe &candidate : candidates) {
        if (candidate.recipe.dishRole != QLatin1String("breakfast"))
            dessertBreakfastCandidates.append(candidate);
    }
    ScoredRecipe cake;
    cake.recipe = recipe(id++, QStringLiteral("松饼"), QStringLiteral("dessert"), 280);
    cake.baseScore = 50.0;
    dessertBreakfastCandidates.append(cake);
    NPGenerator dessertBreakfastGenerator(user, dessertBreakfastCandidates);
    const RecommendResult dessertBreakfastPlan = dessertBreakfastGenerator.generateDailyPlan();
    const bool dessertUsedAtBreakfast = dessertBreakfastPlan.valid
        && !dessertBreakfastPlan.breakfast.dishes.isEmpty()
        && dessertBreakfastPlan.breakfast.dishes.first().dishRole == QLatin1String("dessert")
        && balanced(dessertBreakfastPlan.lunch)
        && balanced(dessertBreakfastPlan.dinner);
    if (!dessertUsedAtBreakfast) {
        qCritical() << "dessert breakfast fallback invalid";
        return 1;
    }
    qInfo() << "Dessert can be breakfast but not lunch/dinner staple";

    // 高蛋白/营养加分不能越过硬热量预算。模拟数据库中 900 kcal 级菜品，
    // 连续生成多次，确保随机候选也不会再产生严重超标方案。
    for (const QString &role : {QStringLiteral("meat"), QStringLiteral("vegetable"),
                                QStringLiteral("staple"), QStringLiteral("drink")}) {
        for (int i = 0; i < 6; ++i) {
            ScoredRecipe high;
            high.recipe = recipe(id++, QStringLiteral("高热量候选%1-%2").arg(role).arg(i),
                                 role, 820.0 + i * 35.0);
            high.recipe.totalProtein = 55.0;
            high.baseScore = 200.0;
            high.nutritionBoost = 100.0;
            candidates.append(high);
        }
    }
    NPGenerator guardedGenerator(user, candidates);
    for (int run = 0; run < 30; ++run) {
        const RecommendResult guarded = guardedGenerator.generateDailyPlan();
        const double calories = guarded.breakfast.totalCalories()
                                + guarded.lunch.totalCalories()
                                + guarded.dinner.totalCalories();
        if (!guarded.valid || calories > user.calorieTarget * 1.10) {
            qCritical() << "calorie guard failed" << run << calories;
            return 1;
        }
    }
    qInfo() << "NP hard calorie budget passed";
    return 0;
}
