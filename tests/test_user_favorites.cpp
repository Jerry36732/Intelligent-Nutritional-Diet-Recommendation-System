#include "../src/dao/DatabaseManager.h"
#include "../src/dao/FoodDAO.h"
#include "../src/dao/RecipeDAO.h"
#include "../src/dao/UserDAO.h"
#include "../src/entities/User.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>

namespace {
int failures = 0;

void expect(bool condition, const char *message)
{
    if (condition)
        qInfo() << "OK  :" << message;
    else {
        qCritical() << "FAIL:" << message;
        ++failures;
    }
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        qCritical() << "usage: test_user_favorites <source diet.db>";
        return 2;
    }

    QTemporaryDir temp;
    const QString dbPath = temp.filePath(QStringLiteral("favorites-test.db"));
    if (!temp.isValid() || !QFile::copy(QString::fromLocal8Bit(argv[1]), dbPath)) {
        qCritical() << "cannot prepare temporary database";
        return 2;
    }
    expect(DatabaseManager::getInstance().open(dbPath), "数据库副本可打开并完成迁移");

    User newcomer;
    newcomer.name = QStringLiteral("__v5_default_user_test__");
    newcomer.gender = QStringLiteral("male");
    newcomer.goal = QStringLiteral("maintain");
    newcomer.height = 170;
    newcomer.weight = 60;
    newcomer.calorieTarget = 1900;
    newcomer.passwordHash = QStringLiteral("test-only");
    expect(UserDAO().insertUser(newcomer), "可创建测试新用户");
    newcomer = UserDAO().findByName(newcomer.name);
    expect(newcomer.id > 0, "可重新读取测试新用户");
    expect(newcomer.preferences.isEmpty(), "新用户饮食偏好为空");
    expect(newcomer.dietaryChoices.isEmpty(), "新用户饮食选择为空");
    expect(newcomer.foodIntolerances.isEmpty(), "新用户食物不耐受为空");
    expect(newcomer.nutritionalDeficiencies.isEmpty(), "新用户营养缺乏为空");
    expect(newcomer.allergies.isEmpty() && newcomer.allergens.isEmpty(), "新用户过敏原为空");
    expect(newcomer.medicalConditions.isEmpty(), "新用户医疗状况为空");
    expect(RecipeDAO().findFavorites(newcomer.id).isEmpty(), "新用户收藏食谱为 0");
    expect(FoodDAO().findFavorites(newcomer.id).isEmpty(), "新用户收藏食材为 0");

    const QList<Food> localizedFoods = FoodDAO().findAll(3000);
    bool rawUsdaNameVisible = false;
    bool localizedWholeEggFound = false;
    for (const Food &food : localizedFoods) {
        rawUsdaNameVisible = rawUsdaNameVisible
            || food.name.startsWith(QStringLiteral("[USDA]"), Qt::CaseInsensitive);
        localizedWholeEggFound = localizedWholeEggFound
            || (food.name.contains(QStringLiteral("鸡蛋"))
                && food.name.contains(QStringLiteral("全蛋")));
    }
    expect(!rawUsdaNameVisible, "食材表不会显示 USDA 英文原名");
    expect(localizedWholeEggFound, "USDA 全蛋食材已规范翻译为中文");

    QSqlQuery ids(DatabaseManager::getInstance().database());
    QList<int> recipeIds;
    ids.exec(QStringLiteral("SELECT id FROM recipes ORDER BY id LIMIT 2"));
    while (ids.next()) recipeIds << ids.value(0).toInt();
    QList<int> foodIds;
    ids.exec(QStringLiteral("SELECT id FROM foods ORDER BY id LIMIT 2"));
    while (ids.next()) foodIds << ids.value(0).toInt();
    expect(recipeIds.size() == 2 && foodIds.size() == 2, "测试库包含至少两道食谱和两种食材");

    if (recipeIds.size() == 2) {
        RecipeDAO recipes;
        expect(recipes.setFavorite(newcomer.id, recipeIds[0], true), "收藏第一道食谱");
        expect(recipes.setFavorite(newcomer.id, recipeIds[1], true), "收藏第二道食谱");
        expect(recipes.findFavorites(newcomer.id).size() == 2, "两道食谱独立收藏且不重复");
        expect(recipes.setFavorite(newcomer.id, recipeIds[0], false), "仅取消第一道食谱");
        expect(!recipes.isFavorite(newcomer.id, recipeIds[0])
                   && recipes.isFavorite(newcomer.id, recipeIds[1]),
               "取消一道食谱不影响另一道");
    }
    if (foodIds.size() == 2) {
        FoodDAO foods;
        expect(foods.setFavorite(newcomer.id, foodIds[0], true), "收藏第一种食材");
        expect(foods.setFavorite(newcomer.id, foodIds[1], true), "收藏第二种食材");
        expect(foods.findFavorites(newcomer.id).size() == 2, "两种食材独立收藏且不重复");
        expect(foods.setFavorite(newcomer.id, foodIds[0], false), "仅取消第一种食材");
        expect(!foods.isFavorite(newcomer.id, foodIds[0])
                   && foods.isFavorite(newcomer.id, foodIds[1]),
               "取消一种食材不影响另一种");
    }

    QSqlQuery shape(DatabaseManager::getInstance().database());
    expect(shape.exec(QStringLiteral(
               "SELECT COUNT(*) FROM user_favorites WHERE user_id=%1 "
               "AND item_type IN ('recipe','ingredient')").arg(newcomer.id))
               && shape.next() && shape.value(0).toInt() == 2,
           "统一收藏表保存 userId、类型、itemId 且结果正确");

    return failures == 0 ? 0 : 1;
}
