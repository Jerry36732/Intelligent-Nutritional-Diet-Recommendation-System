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
    q.prepare(QStringLiteral("SELECT %1 FROM recipes WHERE category = :category "
                             "AND IFNULL(source_ref,'') NOT LIKE 'USER:%%' ORDER BY id")
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
                  "SELECT %1 FROM recipes WHERE IFNULL(dish_role, 'mixed') = :role "
                  "AND IFNULL(source_ref,'') NOT LIKE 'USER:%%' ORDER BY id")
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
    if (!q.exec(QStringLiteral("SELECT %1 FROM recipes "
                               "WHERE IFNULL(source_ref,'') NOT LIKE 'USER:%%' ORDER BY category, id")
                    .arg(QLatin1String(kRecipeColumns)))) {
        qWarning() << "RecipeDAO::findAll:" << q.lastError().text();
        return list;
    }
    while (q.next())
        list.append(mapRow(q));
    return list;
}

QList<Recipe> RecipeDAO::browse(const QString &search, const QString &filterKey,
                                int userId, int offset, int limit, int *total)
{
    QList<Recipe> list;
    QSqlDatabase db = DatabaseManager::getInstance().database();
    QString roleClause;
    if (filterKey == QLatin1String("breakfast"))
        roleClause = QStringLiteral(" AND r.category='早餐'");
    else if (filterKey == QLatin1String("lunch"))
        roleClause = QStringLiteral(" AND r.category='午餐'");
    else if (filterKey == QLatin1String("dinner"))
        roleClause = QStringLiteral(" AND r.category='晚餐'");
    else if (filterKey == QLatin1String("staple") || filterKey == QLatin1String("meat")
             || filterKey == QLatin1String("vegetable") || filterKey == QLatin1String("soup"))
        roleClause = QStringLiteral(" AND IFNULL(r.dish_role,'mixed')=:role");
    else if (filterKey == QLatin1String("dessert"))
        roleClause = QStringLiteral(
            " AND (IFNULL(r.dish_role,'mixed')='dessert' OR r.name LIKE '%蛋糕%' "
            "OR r.name LIKE '%甜品%' OR r.name LIKE '%糖水%' OR r.name LIKE '%布丁%' "
            "OR r.name LIKE '%酸奶杯%' OR r.name LIKE '%甜羹%')");

    const QString visibleClause = QStringLiteral(
        "(IFNULL(r.source_ref,'') NOT LIKE 'USER:%%' OR EXISTS("
        "SELECT 1 FROM user_recipe_library ul WHERE ul.user_id=:uid AND ul.recipe_id=r.id))");
    const QString searchClause = QStringLiteral(
        " AND (:search='' OR r.name LIKE :pattern OR EXISTS("
        "SELECT 1 FROM recipe_foods rf JOIN foods f ON f.id=rf.food_id "
        "WHERE rf.recipe_id=r.id AND COALESCE(NULLIF(rf.display_name,''),f.name) LIKE :pattern))");
    const QString where = QStringLiteral(" WHERE ") + visibleClause + searchClause + roleClause;

    auto bindCommon = [&](QSqlQuery &query) {
        query.bindValue(QStringLiteral(":uid"), userId);
        query.bindValue(QStringLiteral(":search"), search.trimmed());
        query.bindValue(QStringLiteral(":pattern"), QStringLiteral("%%1%").arg(search.trimmed()));
        if (roleClause.contains(QStringLiteral(":role")))
            query.bindValue(QStringLiteral(":role"), filterKey);
    };

    if (total) {
        QSqlQuery count(db);
        count.prepare(QStringLiteral("SELECT COUNT(*) FROM recipes r") + where);
        bindCommon(count);
        *total = count.exec() && count.next() ? count.value(0).toInt() : 0;
    }

    const QString columns = QStringLiteral(
        "r.id AS id,r.name AS name,r.category AS category,r.steps AS steps,"
        "r.cook_minutes AS cook_minutes,r.accent AS accent,"
        "r.total_calories AS total_calories,r.total_protein AS total_protein,"
        "r.total_carbs AS total_carbs,r.total_fat AS total_fat,"
        "IFNULL(r.dish_role,'mixed') AS dish_role,IFNULL(r.total_weight,0) AS total_weight,"
        "IFNULL(r.per100_calories,0) AS per100_calories,"
        "IFNULL(r.per100_protein,0) AS per100_protein,"
        "IFNULL(r.per100_carbs,0) AS per100_carbs,IFNULL(r.per100_fat,0) AS per100_fat");
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT %1 FROM recipes r%2 "
                                 "ORDER BY CASE WHEN IFNULL(r.source_ref,'') LIKE 'USER:%%' THEN 0 ELSE 1 END,"
                                 "r.name LIMIT :limit OFFSET :offset")
                      .arg(columns, where));
    bindCommon(query);
    query.bindValue(QStringLiteral(":limit"), qMax(1, limit));
    query.bindValue(QStringLiteral(":offset"), qMax(0, offset));
    if (!query.exec()) {
        qWarning() << "RecipeDAO::browse:" << query.lastError().text();
        return list;
    }
    while (query.next())
        list.append(mapRow(query));
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
        "SELECT COALESCE(NULLIF(rf.display_name, ''), f.name) AS food_name, rf.quantity, "
        "COALESCE(NULLIF(rf.quantity_text, ''), printf('%g g', rf.quantity)) AS quantity_text "
        "FROM recipe_foods rf "
        "JOIN foods f ON f.id = rf.food_id "
        "WHERE rf.recipe_id = :rid "
        // 保持原始食谱的原料顺序：主料优先，后补调味料随后。
        // 按食材名称排序会把鸡肝、大虾头等主料挤到滚动区下方。
        "ORDER BY rf.rowid"));
    q.bindValue(QStringLiteral(":rid"), recipeId);
    if (!q.exec()) {
        qWarning() << "RecipeDAO::getIngredients:" << q.lastError().text();
        return list;
    }
    while (q.next()) {
        RecipeIngredient ing;
        ing.foodName = RecipeText::normalizeIngredientName(
            q.value(QStringLiteral("food_name")).toString());
        ing.quantity = q.value(QStringLiteral("quantity")).toDouble();
        ing.quantityText = q.value(QStringLiteral("quantity_text")).toString();
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
                  "INNER JOIN user_favorites fav ON fav.item_id = r.id "
                  "WHERE fav.user_id = :uid AND fav.item_type = 'recipe' "
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
        "SELECT 1 FROM user_favorites WHERE user_id = :uid "
        "AND item_type = 'recipe' AND item_id = :rid LIMIT 1"));
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
    return setFavorite(userId, recipeId, !isFavorite(userId, recipeId));
}

bool RecipeDAO::setFavorite(int userId, int recipeId, bool favorite)
{
    if (userId <= 0 || recipeId <= 0)
        return false;
    QSqlDatabase db = DatabaseManager::getInstance().database();
    if (!favorite) {
        if (!db.transaction())
            return false;
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "DELETE FROM user_favorites WHERE user_id = :uid "
            "AND item_type = 'recipe' AND item_id = :rid"));
        q.bindValue(QStringLiteral(":uid"), userId);
        q.bindValue(QStringLiteral(":rid"), recipeId);
        if (!q.exec()) {
            qWarning() << "RecipeDAO::toggleFavorite delete:" << q.lastError().text();
            db.rollback();
            return false;
        }
        QSqlQuery legacy(db);
        legacy.prepare(QStringLiteral(
            "DELETE FROM favorites WHERE user_id = :uid AND recipe_id = :rid"));
        legacy.bindValue(QStringLiteral(":uid"), userId);
        legacy.bindValue(QStringLiteral(":rid"), recipeId);
        return legacy.exec() && db.commit();
    }

    if (!db.transaction())
        return false;
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO user_favorites (user_id,item_type,item_id,created_at) "
        "VALUES (:uid,'recipe',:rid,datetime('now','localtime'))"));
    q.bindValue(QStringLiteral(":uid"), userId);
    q.bindValue(QStringLiteral(":rid"), recipeId);
    if (!q.exec()) {
        qWarning() << "RecipeDAO::toggleFavorite insert:" << q.lastError().text();
        db.rollback();
        return false;
    }
    QSqlQuery legacy(db);
    legacy.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO favorites(user_id,recipe_id,created_at) "
        "VALUES(:uid,:rid,datetime('now','localtime'))"));
    legacy.bindValue(QStringLiteral(":uid"), userId);
    legacy.bindValue(QStringLiteral(":rid"), recipeId);
    return legacy.exec() && db.commit();
}
