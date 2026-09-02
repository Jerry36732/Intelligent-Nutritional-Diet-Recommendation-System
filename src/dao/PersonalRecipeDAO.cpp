#include "PersonalRecipeDAO.h"

#include "DatabaseManager.h"
#include "RecipeDAO.h"
#include "../services/ShoppingListService.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
QVariantMap lookupFood(QSqlDatabase db, const QString &sourceName)
{
    QString name = sourceName.trimmed();
    QSqlQuery exact(db);
    exact.prepare(QStringLiteral(
        "SELECT id,name,calories,protein,fat,carbs FROM foods "
        "WHERE name=:name OR name LIKE :prefix ORDER BY CASE WHEN name=:name THEN 0 ELSE 1 END,id LIMIT 1"));
    exact.bindValue(QStringLiteral(":name"), name);
    exact.bindValue(QStringLiteral(":prefix"), name + QStringLiteral("%"));
    if (exact.exec() && exact.next()) {
        QVariantMap food;
        for (const QString &column : {QStringLiteral("id"), QStringLiteral("name"),
                                      QStringLiteral("calories"), QStringLiteral("protein"),
                                      QStringLiteral("fat"), QStringLiteral("carbs")})
            food.insert(column, exact.value(column));
        return food;
    }

    QString fallback;
    const QList<QPair<QStringList, QString>> families = {
        {{QStringLiteral("鸡腿"), QStringLiteral("鸡胸"), QStringLiteral("鸡肉")}, QStringLiteral("鸡")},
        {{QStringLiteral("牛腩"), QStringLiteral("牛里脊"), QStringLiteral("牛肉")}, QStringLiteral("牛肉")},
        {{QStringLiteral("猪肉"), QStringLiteral("肉末"), QStringLiteral("瘦肉")}, QStringLiteral("猪肉")},
        {{QStringLiteral("鱼"), QStringLiteral("鱼片")}, QStringLiteral("鱼")},
        {{QStringLiteral("虾"), QStringLiteral("虾仁")}, QStringLiteral("虾")},
        {{QStringLiteral("青菜"), QStringLiteral("菜心"), QStringLiteral("油菜")}, QStringLiteral("青菜")},
        {{QStringLiteral("番茄"), QStringLiteral("西红柿")}, QStringLiteral("番茄")},
        {{QStringLiteral("米饭"), QStringLiteral("大米")}, QStringLiteral("米饭")},
        {{QStringLiteral("鸡蛋"), QStringLiteral("蛋液")}, QStringLiteral("鸡蛋")},
    };
    for (const auto &family : families) {
        for (const QString &term : family.first) {
            if (name.contains(term)) {
                fallback = family.second;
                break;
            }
        }
        if (!fallback.isEmpty())
            break;
    }
    if (!fallback.isEmpty()) {
        QSqlQuery similar(db);
        similar.prepare(QStringLiteral(
            "SELECT id,name,calories,protein,fat,carbs FROM foods WHERE name LIKE :like ORDER BY id LIMIT 1"));
        similar.bindValue(QStringLiteral(":like"), QStringLiteral("%") + fallback + QStringLiteral("%"));
        if (similar.exec() && similar.next()) {
            // 为原始名称建立独立“同类营养”食材行。recipe_foods 以
            // (recipe_id, food_id) 唯一；若鸡腿、鸡胸都直接复用同一“鸡”行，
            // 后写入项会覆盖前一主料，违反“主料必须完整”的要求。
            const QString aliasName = name + QStringLiteral("（同类营养）");
            QSqlQuery alias(db);
            alias.prepare(QStringLiteral(
                "SELECT id,name,calories,protein,fat,carbs FROM foods WHERE name=:name LIMIT 1"));
            alias.bindValue(QStringLiteral(":name"), aliasName);
            if (!alias.exec() || !alias.next()) {
                const QByteArray aliasDigest = QCryptographicHash::hash(
                    (QStringLiteral("similar:") + name).toUtf8(), QCryptographicHash::Sha1).toHex().left(7);
                int aliasSourceId = -31000000 - aliasDigest.toInt(nullptr, 16);
                while (true) {
                    QSqlQuery occupied(db);
                    occupied.prepare(QStringLiteral("SELECT 1 FROM foods WHERE source_id=:sid"));
                    occupied.bindValue(QStringLiteral(":sid"), aliasSourceId);
                    if (!occupied.exec() || !occupied.next())
                        break;
                    --aliasSourceId;
                }
                QSqlQuery createAlias(db);
                createAlias.prepare(QStringLiteral(
                    "INSERT INTO foods(source_id,name,calories,protein,fat,carbs,unit,source,category_label) "
                    "VALUES(:sid,:name,:cal,:protein,:fat,:carbs,'100g',:source,'同类营养近似')"));
                createAlias.bindValue(QStringLiteral(":sid"), aliasSourceId);
                createAlias.bindValue(QStringLiteral(":name"), aliasName);
                createAlias.bindValue(QStringLiteral(":cal"), similar.value(QStringLiteral("calories")));
                createAlias.bindValue(QStringLiteral(":protein"), similar.value(QStringLiteral("protein")));
                createAlias.bindValue(QStringLiteral(":fat"), similar.value(QStringLiteral("fat")));
                createAlias.bindValue(QStringLiteral(":carbs"), similar.value(QStringLiteral("carbs")));
                createAlias.bindValue(QStringLiteral(":source"),
                                      QStringLiteral("个人食谱同类营养近似：%1")
                                          .arg(similar.value(QStringLiteral("name")).toString()));
                if (!createAlias.exec())
                    return {};
                alias.prepare(QStringLiteral(
                    "SELECT id,name,calories,protein,fat,carbs FROM foods WHERE id=:id"));
                alias.bindValue(QStringLiteral(":id"), createAlias.lastInsertId());
                if (!alias.exec() || !alias.next())
                    return {};
            }
            QVariantMap food;
            for (const QString &column : {QStringLiteral("id"), QStringLiteral("name"),
                                          QStringLiteral("calories"), QStringLiteral("protein"),
                                          QStringLiteral("fat"), QStringLiteral("carbs")})
                food.insert(column, alias.value(column));
            food.insert(QStringLiteral("fallback"), true);
            food.insert(QStringLiteral("fallbackSource"), similar.value(QStringLiteral("name")));
            return food;
        }
    }

    const bool ignored = ShoppingListService::isCommonPantrySeasoning(name)
                         || ShoppingListService::isSpice(name);
    const QByteArray digest = QCryptographicHash::hash(name.toUtf8(), QCryptographicHash::Sha1).toHex().left(7);
    int sourceId = -30000000 - digest.toInt(nullptr, 16);
    QSqlQuery findExisting(db);
    findExisting.prepare(QStringLiteral("SELECT id,name,calories,protein,fat,carbs FROM foods WHERE source_id=:sid"));
    findExisting.bindValue(QStringLiteral(":sid"), sourceId);
    if (findExisting.exec() && findExisting.next()) {
        QVariantMap food;
        for (const QString &column : {QStringLiteral("id"), QStringLiteral("name"),
                                      QStringLiteral("calories"), QStringLiteral("protein"),
                                      QStringLiteral("fat"), QStringLiteral("carbs")})
            food.insert(column, findExisting.value(column));
        return food;
    }
    while (true) {
        QSqlQuery occupied(db);
        occupied.prepare(QStringLiteral("SELECT 1 FROM foods WHERE source_id=:sid"));
        occupied.bindValue(QStringLiteral(":sid"), sourceId);
        if (!occupied.exec() || !occupied.next())
            break;
        --sourceId;
    }
    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(
        "INSERT INTO foods(source_id,name,calories,protein,fat,carbs,unit,source,category_label) "
        "VALUES(:sid,:name,:cal,:protein,:fat,:carbs,'100g',:source,'个人食谱原料')"));
    insert.bindValue(QStringLiteral(":sid"), sourceId);
    insert.bindValue(QStringLiteral(":name"), ignored ? QStringLiteral("微量调料：") + name : name);
    insert.bindValue(QStringLiteral(":cal"), ignored ? QVariant(0.0) : QVariant());
    insert.bindValue(QStringLiteral(":protein"), ignored ? QVariant(0.0) : QVariant());
    insert.bindValue(QStringLiteral(":fat"), ignored ? QVariant(0.0) : QVariant());
    insert.bindValue(QStringLiteral(":carbs"), ignored ? QVariant(0.0) : QVariant());
    insert.bindValue(QStringLiteral(":source"), ignored
                         ? QStringLiteral("个人食谱微量调味料：营养忽略")
                         : QStringLiteral("个人食谱原料：待补充或采用同类营养"));
    if (!insert.exec())
        return {};
    QVariantMap food;
    food.insert(QStringLiteral("id"), insert.lastInsertId());
    food.insert(QStringLiteral("name"), name);
    food.insert(QStringLiteral("calories"), ignored ? QVariant(0.0) : QVariant());
    food.insert(QStringLiteral("protein"), ignored ? QVariant(0.0) : QVariant());
    food.insert(QStringLiteral("fat"), ignored ? QVariant(0.0) : QVariant());
    food.insert(QStringLiteral("carbs"), ignored ? QVariant(0.0) : QVariant());
    food.insert(QStringLiteral("unresolved"), !ignored);
    return food;
}
} // namespace

int PersonalRecipeDAO::create(int userId, const PersonalRecipeDraft &draft, QString *errorMessage) const
{
    auto fail = [&](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return 0;
    };
    if (userId <= 0 || draft.name.trimmed().isEmpty() || draft.ingredients.isEmpty())
        return fail(QStringLiteral("菜名和原料不能为空。"));
    QSqlDatabase db = DatabaseManager::getInstance().database();
    if (!db.isOpen() || !db.transaction())
        return fail(QStringLiteral("数据库不可用。"));

    QSqlQuery recipe(db);
    recipe.prepare(QStringLiteral(
        "INSERT INTO recipes(name,category,steps,cook_minutes,accent,dish_role,source,source_url,source_ref) "
        "VALUES(:name,:category,:steps,:minutes,'green',:role,:source,:url,:ref)"));
    recipe.bindValue(QStringLiteral(":name"), draft.name.trimmed());
    recipe.bindValue(QStringLiteral(":category"), draft.category.isEmpty() ? QStringLiteral("午餐") : draft.category);
    recipe.bindValue(QStringLiteral(":role"), draft.dishRole.isEmpty() ? QStringLiteral("mixed")
                                                                        : draft.dishRole);
    recipe.bindValue(QStringLiteral(":steps"), draft.steps.trimmed());
    recipe.bindValue(QStringLiteral(":minutes"), qMax(1, draft.cookMinutes));
    const QString sourceLabel = draft.sourceType == QLatin1String("web")
        ? QStringLiteral("用户网页导入")
        : draft.sourceType == QLatin1String("dna")
            ? QStringLiteral("用户AI改造") : QStringLiteral("用户手动创建");
    recipe.bindValue(QStringLiteral(":source"), sourceLabel);
    recipe.bindValue(QStringLiteral(":url"), draft.sourceUrl.trimmed());
    recipe.bindValue(QStringLiteral(":ref"), QStringLiteral("USER:%1").arg(userId));
    if (!recipe.exec()) {
        db.rollback();
        return fail(recipe.lastError().text());
    }
    const int recipeId = recipe.lastInsertId().toInt();
    double weight = 0.0;
    double calories = 0.0, protein = 0.0, fat = 0.0, carbs = 0.0;
    bool unresolved = false;
    for (const PersonalRecipeIngredient &ingredient : draft.ingredients) {
        const QString name = ingredient.name.trimmed();
        const double quantity = qMax(0.1, ingredient.quantity);
        if (name.isEmpty())
            continue;
        const QVariantMap food = lookupFood(db, name);
        if (food.isEmpty()) {
            db.rollback();
            return fail(QStringLiteral("无法保存原料：%1").arg(name));
        }
        const bool ignored = ShoppingListService::isCommonPantrySeasoning(name)
                             || ShoppingListService::isSpice(name);
        QString sourceText = name;
        if (food.value(QStringLiteral("fallback")).toBool())
            sourceText += QStringLiteral("【同类食材近似：%1】")
                              .arg(food.value(QStringLiteral("fallbackSource")).toString());
        if (ignored)
            sourceText += QStringLiteral("【营养忽略】");
        QSqlQuery link(db);
        link.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO recipe_foods(recipe_id,food_id,quantity,display_name,source_text,quantity_text) "
            "VALUES(:recipe,:food,:quantity,:display,:source,:quantityText)"));
        link.bindValue(QStringLiteral(":recipe"), recipeId);
        link.bindValue(QStringLiteral(":food"), food.value(QStringLiteral("id")));
        link.bindValue(QStringLiteral(":quantity"), quantity);
        link.bindValue(QStringLiteral(":display"), name);
        link.bindValue(QStringLiteral(":source"), sourceText);
        link.bindValue(QStringLiteral(":quantityText"), ingredient.quantityText.trimmed());
        if (!link.exec()) {
            db.rollback();
            return fail(link.lastError().text());
        }
        weight += quantity;
        if (ignored)
            continue;
        if (food.value(QStringLiteral("calories")).isNull()
            || food.value(QStringLiteral("protein")).isNull()
            || food.value(QStringLiteral("fat")).isNull()
            || food.value(QStringLiteral("carbs")).isNull()) {
            unresolved = true;
            continue;
        }
        const double factor = quantity / 100.0;
        calories += food.value(QStringLiteral("calories")).toDouble() * factor;
        protein += food.value(QStringLiteral("protein")).toDouble() * factor;
        fat += food.value(QStringLiteral("fat")).toDouble() * factor;
        carbs += food.value(QStringLiteral("carbs")).toDouble() * factor;
    }

    QSqlQuery totals(db);
    totals.prepare(QStringLiteral(
        "UPDATE recipes SET total_weight=:weight,total_calories=:cal, total_protein=:protein,"
        "total_fat=:fat,total_carbs=:carbs,per100_calories=:pcal,per100_protein=:pprotein,"
        "per100_fat=:pfat,per100_carbs=:pcarbs,nutrition_verified_at=:status WHERE id=:id"));
    totals.bindValue(QStringLiteral(":weight"), weight);
    totals.bindValue(QStringLiteral(":cal"), calories);
    totals.bindValue(QStringLiteral(":protein"), protein);
    totals.bindValue(QStringLiteral(":fat"), fat);
    totals.bindValue(QStringLiteral(":carbs"), carbs);
    totals.bindValue(QStringLiteral(":pcal"), weight > 0 ? calories * 100.0 / weight : 0.0);
    totals.bindValue(QStringLiteral(":pprotein"), weight > 0 ? protein * 100.0 / weight : 0.0);
    totals.bindValue(QStringLiteral(":pfat"), weight > 0 ? fat * 100.0 / weight : 0.0);
    totals.bindValue(QStringLiteral(":pcarbs"), weight > 0 ? carbs * 100.0 / weight : 0.0);
    totals.bindValue(QStringLiteral(":status"), unresolved
        ? QStringLiteral("个人食谱：部分原料采用待补充营养值；主料已完整保留")
        : QStringLiteral("个人食谱：已按现有食材营养库计算，微量调味料已忽略"));
    totals.bindValue(QStringLiteral(":id"), recipeId);
    if (!totals.exec()) {
        db.rollback();
        return fail(totals.lastError().text());
    }

    QSqlQuery library(db);
    library.prepare(QStringLiteral(
        "INSERT INTO user_recipe_library(user_id,recipe_id,source_type,source_url) "
        "VALUES(:user,:recipe,:type,:url)"));
    library.bindValue(QStringLiteral(":user"), userId);
    library.bindValue(QStringLiteral(":recipe"), recipeId);
    library.bindValue(QStringLiteral(":type"), draft.sourceType.isEmpty() ? QStringLiteral("manual") : draft.sourceType);
    library.bindValue(QStringLiteral(":url"), draft.sourceUrl.trimmed());
    if (!library.exec()) {
        db.rollback();
        return fail(library.lastError().text());
    }
    QSqlQuery favorite(db);
    favorite.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO user_favorites(user_id,item_type,item_id,created_at) "
        "VALUES(:user,'recipe',:recipe,datetime('now','localtime'))"));
    favorite.bindValue(QStringLiteral(":user"), userId);
    favorite.bindValue(QStringLiteral(":recipe"), recipeId);
    if (!favorite.exec() || !db.commit()) {
        db.rollback();
        return fail(QStringLiteral("个人食谱已写入但收藏状态保存失败。"));
    }
    return recipeId;
}

QList<Recipe> PersonalRecipeDAO::findByUser(int userId) const
{
    QList<Recipe> result;
    QSqlQuery q(DatabaseManager::getInstance().database());
    q.prepare(QStringLiteral(
        "SELECT recipe_id FROM user_recipe_library WHERE user_id=:user ORDER BY created_at DESC"));
    q.bindValue(QStringLiteral(":user"), userId);
    if (!q.exec())
        return result;
    RecipeDAO recipes;
    while (q.next()) {
        const Recipe recipe = recipes.findById(q.value(0).toInt());
        if (recipe.isValid())
            result.append(recipe);
    }
    return result;
}

bool PersonalRecipeDAO::isPersonal(int userId, int recipeId) const
{
    QSqlQuery q(DatabaseManager::getInstance().database());
    q.prepare(QStringLiteral(
        "SELECT 1 FROM user_recipe_library WHERE user_id=:user AND recipe_id=:recipe LIMIT 1"));
    q.bindValue(QStringLiteral(":user"), userId);
    q.bindValue(QStringLiteral(":recipe"), recipeId);
    return q.exec() && q.next();
}
