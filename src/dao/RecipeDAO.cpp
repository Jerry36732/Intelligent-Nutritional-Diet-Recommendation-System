#include "RecipeDAO.h"
#include "DatabaseManager.h"

#include "../services/RecipeText.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
const char *kRecipeColumns =
    "id, name, category, steps, cook_minutes, accent, "
    "total_calories, total_protein, total_carbs, total_fat, "
    "IFNULL(dish_role, 'mixed') AS dish_role, "
    "IFNULL(total_weight, 0) AS total_weight, IFNULL(per100_calories, 0) AS per100_calories, "
    "IFNULL(per100_protein, 0) AS per100_protein, IFNULL(per100_carbs, 0) AS per100_carbs, "
    "IFNULL(per100_fat, 0) AS per100_fat";
}

Recipe RecipeDAO::mapRow(const QSqlQuery &query) const
{
    Recipe r;
    r.id = query.value(QStringLiteral("id")).toInt();
    r.name = RecipeText::normalizeName(query.value(QStringLiteral("name")).toString());
    r.category = query.value(QStringLiteral("category")).toString();
    r.dishRole = query.value(QStringLiteral("dish_role")).toString();
    if (r.dishRole.isEmpty())
        r.dishRole = QStringLiteral("mixed");
    r.steps = query.value(QStringLiteral("steps")).toString();
    r.cookMinutes = query.value(QStringLiteral("cook_minutes")).toInt();
    r.accent = query.value(QStringLiteral("accent")).toString();
    r.totalCalories = query.value(QStringLiteral("total_calories")).toDouble();
    r.totalProtein = query.value(QStringLiteral("total_protein")).toDouble();
    r.totalCarbs = query.value(QStringLiteral("total_carbs")).toDouble();
    r.totalFat = query.value(QStringLiteral("total_fat")).toDouble();
    r.totalWeight = query.value(QStringLiteral("total_weight")).toDouble();
    r.per100Calories = query.value(QStringLiteral("per100_calories")).toDouble();
    r.per100Protein = query.value(QStringLiteral("per100_protein")).toDouble();
    r.per100Carbs = query.value(QStringLiteral("per100_carbs")).toDouble();
    r.per100Fat = query.value(QStringLiteral("per100_fat")).toDouble();
    return r;
}

QList<Recipe> RecipeDAO::findByCategory(const QString &category)
{
    QList<Recipe> list;
    QSqlDatabase db = DatabaseManager::getInstance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT %1 FROM recipes WHERE category = :category ORDER BY id")
                  .arg(QLatin1String(kRecipeColumns)));
    q.bindValue(QStringLiteral(":category"), category);
    if (!q.exec()) {
        qWarning() << "RecipeDAO::findByCategory:" << q.lastError().text();
        return list;
    }
    while (q.next())
        list.append(mapRow(q));
    return list;
}

QList<Recipe> RecipeDAO::findByRole(const QString &role)
{
    QList<Recipe> list;
    QSqlDatabase db = DatabaseManager::getInstance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
                  "SELECT %1 FROM recipes WHERE IFNULL(dish_role, 'mixed') = :role ORDER BY id")
                  .arg(QLatin1String(kRecipeColumns)));
    q.bindValue(QStringLiteral(":role"), role);
    if (!q.exec()) {
        qWarning() << "RecipeDAO::findByRole:" << q.lastError().text();
        return list;
    }
    while (q.next())
        list.append(mapRow(q));
    return list;
}

QList<Recipe> RecipeDAO::findAll()
{
    QList<Recipe> list;
    QSqlDatabase db = DatabaseManager::getInstance().database();
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT %1 FROM recipes ORDER BY category, id")
                    .arg(QLatin1String(kRecipeColumns)))) {
        qWarning() << "RecipeDAO::findAll:" << q.lastError().text();
        return list;
    }
    while (q.next())
        list.append(mapRow(q));
    return list;
}

Recipe RecipeDAO::findById(int id)
{
    QSqlDatabase db = DatabaseManager::getInstance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT %1 FROM recipes WHERE id = :id").arg(QLatin1String(kRecipeColumns)));
    q.bindValue(QStringLiteral(":id"), id);
    if (!q.exec()) {
        qWarning() << "RecipeDAO::findById:" << q.lastError().text();
        return Recipe{};
    }
    if (q.next()) {
        Recipe r = mapRow(q);
        r.ingredients = getIngredients(r.id);
        return r;
    }
    return Recipe{};
}

QList<RecipeIngredient> RecipeDAO::getIngredients(int recipeId)
{
    QList<RecipeIngredient> list;
    QSqlDatabase db = DatabaseManager::getInstance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT COALESCE(NULLIF(rf.display_name, ''), f.name) AS food_name, rf.quantity "
        "FROM recipe_foods rf "
        "JOIN foods f ON f.id = rf.food_id "
        "WHERE rf.recipe_id = :rid "
        "ORDER BY f.name"));
    q.bindValue(QStringLiteral(":rid"), recipeId);
    if (!q.exec()) {
        qWarning() << "RecipeDAO::getIngredients:" << q.lastError().text();
        return list;
    }
    while (q.next()) {
        RecipeIngredient ing;
        ing.foodName = q.value(QStringLiteral("food_name")).toString();
        ing.quantity = q.value(QStringLiteral("quantity")).toDouble();
        list.append(ing);
    }
    return list;
}

QList<Recipe> RecipeDAO::findFavorites(int userId)
{
    QList<Recipe> list;
    QSqlDatabase db = DatabaseManager::getInstance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
                  "SELECT r.id, r.name, r.category, r.steps, r.cook_minutes, r.accent, "
                  "r.total_calories, r.total_protein, r.total_carbs, r.total_fat, "
                  "IFNULL(r.dish_role, 'mixed') AS dish_role, "
                  "IFNULL(r.total_weight, 0) AS total_weight, IFNULL(r.per100_calories, 0) AS per100_calories, "
                  "IFNULL(r.per100_protein, 0) AS per100_protein, IFNULL(r.per100_carbs, 0) AS per100_carbs, "
                  "IFNULL(r.per100_fat, 0) AS per100_fat "
                  "FROM recipes r "
                  "INNER JOIN favorites fav ON fav.recipe_id = r.id "
                  "WHERE fav.user_id = :uid "
                  "ORDER BY r.name"));
    q.bindValue(QStringLiteral(":uid"), userId);
    if (!q.exec()) {
        qWarning() << "RecipeDAO::findFavorites:" << q.lastError().text();
        return list;
    }
    while (q.next())
        list.append(mapRow(q));
    return list;
}

bool RecipeDAO::isFavorite(int userId, int recipeId)
{
    QSqlDatabase db = DatabaseManager::getInstance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT 1 FROM favorites WHERE user_id = :uid AND recipe_id = :rid LIMIT 1"));
    q.bindValue(QStringLiteral(":uid"), userId);
    q.bindValue(QStringLiteral(":rid"), recipeId);
    if (!q.exec()) {
        qWarning() << "RecipeDAO::isFavorite:" << q.lastError().text();
        return false;
    }
    return q.next();
}

bool RecipeDAO::toggleFavorite(int userId, int recipeId)
{
    QSqlDatabase db = DatabaseManager::getInstance().database();
    if (isFavorite(userId, recipeId)) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "DELETE FROM favorites WHERE user_id = :uid AND recipe_id = :rid"));
        q.bindValue(QStringLiteral(":uid"), userId);
        q.bindValue(QStringLiteral(":rid"), recipeId);
        if (!q.exec()) {
            qWarning() << "RecipeDAO::toggleFavorite delete:" << q.lastError().text();
            return false;
        }
        return true;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO favorites (user_id, recipe_id) VALUES (:uid, :rid)"));
    q.bindValue(QStringLiteral(":uid"), userId);
    q.bindValue(QStringLiteral(":rid"), recipeId);
    if (!q.exec()) {
        qWarning() << "RecipeDAO::toggleFavorite insert:" << q.lastError().text();
        return false;
    }
    return true;
}
