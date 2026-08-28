/**
 * 简单 RDSS 过滤单元测试（虚拟用户）
 * 运行：test_rdss.exe
 */
#include "../src/engine/RDSSEngine.h"
#include "../src/entities/Recipe.h"
#include "../src/entities/User.h"

#include <QCoreApplication>
#include <QDebug>

static int g_failed = 0;

static void expect(bool cond, const char *msg)
{
    if (!cond) {
        qWarning() << "FAIL:" << msg;
        ++g_failed;
    } else {
        qInfo() << "OK  :" << msg;
    }
}

static Recipe makeRecipe(int id, const QString &name)
{
    Recipe r;
    r.id = id;
    r.name = name;
    r.category = QStringLiteral("午餐");
    r.dishRole = QStringLiteral("vegetable");
    r.totalCalories = 200;
    r.totalProtein = 10;
    return r;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    User user;
    user.name = QStringLiteral("测试用户");
    user.allergies = {QStringLiteral("豆制品")};
    user.syncAllergenFields();

    const QList<Recipe> recipes = {
        makeRecipe(1, QStringLiteral("清炒菠菜")),
        makeRecipe(2, QStringLiteral("三丝豆腐")),
        makeRecipe(3, QStringLiteral("红烧牛肉")),
        makeRecipe(4, QStringLiteral("家常豆浆")),
        makeRecipe(5, QStringLiteral("蒜蓉西兰花")),
    };

    RDSSEngine rdss(user);
    const QList<Recipe> afterAllergy = rdss.filterByAllergies(recipes);

    bool tofuGone = true;
    bool soyGone = true;
    bool spinachKept = false;
    for (const Recipe &r : afterAllergy) {
        if (r.name.contains(QStringLiteral("豆腐")))
            tofuGone = false;
        if (r.name.contains(QStringLiteral("豆浆")))
            soyGone = false;
        if (r.name.contains(QStringLiteral("菠菜")))
            spinachKept = true;
    }

    expect(tofuGone, "豆制品过敏应排除「三丝豆腐」");
    expect(soyGone, "豆制品过敏应排除「家常豆浆」");
    expect(spinachKept, "应保留「清炒菠菜」");
    expect(afterAllergy.size() >= 2, "过滤后仍有非豆制品候选");

    // 缺铁提升
    User ironUser = user;
    ironUser.allergies.clear();
    ironUser.allergens.clear();
    ironUser.nutritionalDeficiencies = {QStringLiteral("缺铁")};
    RDSSEngine rdssIron(ironUser);
    const QList<ScoredRecipe> scored = rdssIron.process(recipes);
    expect(!scored.isEmpty(), "缺铁用户 process 非空");
    if (!scored.isEmpty()) {
        expect(scored.first().recipe.name.contains(QStringLiteral("菠菜"))
                   || scored.first().nutritionBoost >= 0,
               "含铁食谱应获得营养增强分");
    }

    if (g_failed == 0) {
        qInfo() << "All RDSS tests passed.";
        return 0;
    }
    qWarning() << g_failed << "test(s) failed.";
    return 1;
}
