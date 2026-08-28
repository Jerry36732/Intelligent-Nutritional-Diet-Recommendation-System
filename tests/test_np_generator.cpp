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

static bool balanced(const MealSlot &meal)
{
    int meat = 0, vegetable = 0, staple = 0;
    for (const Recipe &r : meal.dishes) {
        meat += r.dishRole == QStringLiteral("meat");
        vegetable += r.dishRole == QStringLiteral("vegetable");
        staple += r.dishRole == QStringLiteral("staple");
    }
    return meat >= 1 && vegetable >= 1 && staple == 1;
}

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
    const bool ok = plan.valid && balanced(plan.lunch) && balanced(plan.dinner);
    if (!ok) {
        qCritical() << "main meal structure invalid";
        return 1;
    }
    qInfo() << "NP main meal structure passed";
    return 0;
}
