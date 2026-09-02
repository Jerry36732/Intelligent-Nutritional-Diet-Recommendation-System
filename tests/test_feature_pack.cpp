#include "../src/dao/DatabaseManager.h"
#include "../src/dao/FridgeDAO.h"
#include "../src/dao/FoodLogDAO.h"
#include "../src/dao/PersonalRecipeDAO.h"
#include "../src/dao/RecipeDAO.h"
#include "../src/entities/RecommendResult.h"
#include "../src/services/ShoppingListService.h"
#include "../src/services/ShoppingListExportService.h"
#include "../src/services/IngredientMeasureService.h"
#include "../src/services/RecipeText.h"
#include "../src/services/RecipeImageProvider.h"
#include "../src/services/AdaptiveTargetService.h"
#include "../src/services/FlavorFingerprintService.h"
#include "../src/services/HealthDataSyncService.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <cstdio>

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

int main(int argc, char **argv)
{
    std::fprintf(stderr, "feature-pack: application\n");
    std::fflush(stderr);
    QGuiApplication app(argc, argv);
    if (argc < 2)
        return 2;
    QTemporaryDir temp;
    const QString dbPath = temp.filePath(QStringLiteral("feature-pack.db"));
    if (!temp.isValid() || !QFile::copy(QString::fromLocal8Bit(argv[1]), dbPath))
        return 2;

    // 构造真实旧版约束，验证 ensureSchema 会迁移既有用户库，而不只验证新建表。
    const QString legacyConnection = QStringLiteral("feature_pack_legacy_fridge");
    {
        QSqlDatabase legacyDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), legacyConnection);
        legacyDb.setDatabaseName(dbPath);
        if (!legacyDb.open())
            return 2;
        QSqlQuery legacy(legacyDb);
        if (!legacy.exec(QStringLiteral("DROP TABLE IF EXISTS fridge_inventory")))
            return 2;
        if (!legacy.exec(QStringLiteral(
                "CREATE TABLE fridge_inventory ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,user_id INTEGER NOT NULL,"
                "food_name TEXT NOT NULL,quantity REAL NOT NULL DEFAULT 1,unit TEXT,"
                "updated_at TEXT NOT NULL DEFAULT (datetime('now','localtime')),expiry_date TEXT,"
                "UNIQUE(user_id,food_name))")))
            return 2;
        if (!legacy.exec(QStringLiteral(
                "INSERT INTO fridge_inventory(user_id,food_name,quantity,unit,expiry_date) "
                "VALUES(1,'迁移测试食材',100,'g','2026-09-08')")))
            return 2;
        legacy.finish();
        legacyDb.close();
    }
    QSqlDatabase::removeDatabase(legacyConnection);
    std::fprintf(stderr, "feature-pack: database-open\n");
    std::fflush(stderr);
    expect(DatabaseManager::getInstance().open(dbPath), "数据库副本可迁移新功能表");
    std::fprintf(stderr, "feature-pack: database-ready\n");
    std::fflush(stderr);

    QSqlQuery user(DatabaseManager::getInstance().database());
    user.exec(QStringLiteral("SELECT id FROM users ORDER BY id LIMIT 1"));
    int userId = user.next() ? user.value(0).toInt() : 0;
    if (userId <= 0) {
        QSqlQuery createUser(DatabaseManager::getInstance().database());
        createUser.exec(QStringLiteral(
            "INSERT INTO users(name,password_hash) "
            "VALUES('__feature_pack_test_user__','')"));
        userId = createUser.lastInsertId().toInt();
    }
    expect(userId > 0, "测试用户存在");

    QSqlQuery fridgeSchema(DatabaseManager::getInstance().database());
    expect(fridgeSchema.exec(QStringLiteral(
               "SELECT LOWER(REPLACE(REPLACE(sql,' ',''),char(10),'')) FROM sqlite_master "
               "WHERE type='table' AND name='fridge_inventory'"))
               && fridgeSchema.next()
               && !fridgeSchema.value(0).toString().contains(
                   QStringLiteral("unique(user_id,food_name)")),
           "旧冰箱表会移除同名食材唯一约束");
    fridgeSchema.finish();

    FoodLogEntry photoLog;
    photoLog.userId = userId;
    photoLog.eatenAt = QDateTime::currentDateTime();
    photoLog.mealLabel = QStringLiteral("午餐");
    photoLog.foodName = QStringLiteral("测试番茄鸡蛋饭");
    photoLog.servingGrams = 420.0;
    photoLog.calories = 568.0;
    photoLog.protein = 23.4;
    photoLog.carbs = 72.1;
    photoLog.fat = 18.5;
    photoLog.confidence = 0.78;
    photoLog.provider = QStringLiteral("test");
    const QString photoSourcePath = temp.filePath(QStringLiteral("meal-photo.jpg"));
    QFile photoSource(photoSourcePath);
    const bool photoOpened = photoSource.open(QIODevice::WriteOnly);
    expect(photoOpened, "测试餐食图片可创建");
    if (photoOpened)
        photoSource.write("test-photo-data");
    photoSource.close();
    QString photoError;
    const int photoLogId = FoodLogDAO().create(photoLog, photoSourcePath, &photoError);
    expect(photoLogId > 0, "拍照识别营养结果可写入独立饮食日志");
    const QList<FoodLogEntry> recentLogs = FoodLogDAO().recentByUser(userId, 3);
    expect(!recentLogs.isEmpty() && recentLogs.first().foodName == photoLog.foodName,
           "饮食日志重启后可按用户读取");
    expect(!recentLogs.isEmpty() && QFileInfo::exists(recentLogs.first().imagePath),
           "用户确认保存后会持久化餐食图片副本");
    const DailyFoodLogTotals photoTotals =
        FoodLogDAO().totalsForDate(userId, QDate::currentDate());
    expect(photoTotals.count >= 1 && qAbs(photoTotals.calories - 568.0) < 0.01,
           "拍照饮食日志可汇总当日热量");
    const QList<DailyFoodLogPoint> trend = FoodLogDAO().dailyTotals(
        userId, QDate::currentDate().addDays(-6), QDate::currentDate());
    expect(trend.size() == 7 && trend.last().totals.count >= 1
               && qAbs(trend.last().totals.calories - 568.0) < 0.01,
           "饮食日志可生成含空白日期的连续7日趋势");
    const QString persistedPhotoPath = recentLogs.isEmpty() ? QString()
                                                             : recentLogs.first().imagePath;
    QString removeLogError;
    expect(FoodLogDAO().remove(photoLogId, userId, &removeLogError),
           "用户可删除误记的饮食日志");
    bool deletedLogStillExists = false;
    for (const FoodLogEntry &entry : FoodLogDAO().recentByUser(userId, 100))
        deletedLogStillExists = deletedLogStillExists || entry.id == photoLogId;
    expect(!deletedLogStillExists, "删除后饮食日志不再出现在最近记录中");
    expect(persistedPhotoPath.isEmpty() || !QFileInfo::exists(persistedPhotoPath),
           "删除饮食日志时会一并清理其私有图片副本");

    const QPixmap detailImage = RecipeImageProvider::pixmap(
        QStringLiteral("三不粘"), QSize(210, 150));
    expect(!detailImage.isNull() && detailImage.size() == QSize(210, 150),
           "食谱详情图片会生成与容器一致的覆盖图");
    const QImage detailPixels = detailImage.toImage();
    int visiblePixels = 0;
    int contentPixels = 0;
    for (int y = 5; y < detailPixels.height() - 5; ++y) {
        for (int x = 5; x < detailPixels.width() - 5; ++x) {
            const QColor color = detailPixels.pixelColor(x, y);
            if (color.alpha() < 64)
                continue;
            ++visiblePixels;
            if (color.value() < 235 || color.hsvSaturation() > 35)
                ++contentPixels;
        }
    }
    expect(visiblePixels > 0 && contentPixels * 100 / visiblePixels >= 35,
           "带大面积空白边缘的菜品图会先裁边再放大主体");

    expect(RecipeText::normalizeIngredientName(QStringLiteral("植物油公斤"))
                   == QStringLiteral("植物油")
               && RecipeText::normalizeIngredientName(QStringLiteral("香葱3棵"))
                      == QStringLiteral("香葱")
               && RecipeText::normalizeIngredientName(QStringLiteral("调味料：盐1／"))
                      == QStringLiteral("盐")
               && RecipeText::normalizeIngredientName(QStringLiteral("鸡蛋（2个）"))
                      == QStringLiteral("鸡蛋")
               && RecipeText::normalizeIngredientName(QStringLiteral("淀粉l小匙"))
                      == QStringLiteral("淀粉"),
           "食谱原料名会移除误粘连的数量和计量单位");

    auto scalar = [](const QString &sql) {
        QSqlQuery query(DatabaseManager::getInstance().database());
        return query.exec(sql) && query.next() ? query.value(0).toInt() : -1;
    };
    expect(scalar(QStringLiteral(
               "SELECT COUNT(*) FROM (SELECT name FROM recipes "
               "WHERE IFNULL(source_ref,'') NOT LIKE 'USER:%%' GROUP BY name HAVING COUNT(*)>1)"))
               == 0,
           "公共食谱库不存在同名重复记录");
    expect(scalar(QStringLiteral("SELECT COUNT(*) FROM recipes WHERE name='桃仁粥'")) == 1,
           "连续重复的桃仁粥只保留一份完整记录");
    expect(scalar(QStringLiteral(
               "SELECT COUNT(*) FROM recipes WHERE (name LIKE '%肉饼%' OR name LIKE '%海鲜饼%') "
               "AND IFNULL(dish_role,'')<>'meat'"))
               == 0,
           "肉饼和海鲜饼统一归入荤菜");
    expect(scalar(QStringLiteral(
               "SELECT COUNT(*) FROM recipes WHERE (name LIKE '%土豆饼%' OR name LIKE '%香椿饼%') "
               "AND IFNULL(dish_role,'')<>'vegetable'"))
               == 0,
           "土豆饼和香椿饼统一归入素菜");
    expect(scalar(QStringLiteral(
               "SELECT COUNT(*) FROM recipes WHERE (name LIKE '%松饼%' OR name LIKE '%蛋糕%') "
               "AND (IFNULL(dish_role,'')<>'dessert' OR category<>'早餐')"))
               == 0,
           "松饼和蛋糕保留早餐适用性但不进入午晚餐主食池");

    int browseTotal = 0;
    const QList<Recipe> staplePage = RecipeDAO().browse(
        QString(), QStringLiteral("staple"), userId, 0, 12, &browseTotal);
    expect(browseTotal > 0 && !staplePage.isEmpty(), "食谱大全可分页查询主食分类");
    bool stapleRolesValid = true;
    for (const Recipe &item : staplePage)
        stapleRolesValid = stapleRolesValid && item.dishRole == QStringLiteral("staple");
    expect(stapleRolesValid, "主食分类不会混入非主食菜品");
    const QList<Recipe> ingredientSearch = RecipeDAO().browse(
        QStringLiteral("猪二刀肉"), QString(), userId, 0, 12, &browseTotal);
    bool foundCrabRecipe = false;
    for (const Recipe &item : ingredientSearch)
        foundCrabRecipe = foundCrabRecipe || item.name == QStringLiteral("回锅肉炒蟹");
    expect(foundCrabRecipe, "食谱大全支持按食材搜索对应食谱");
    const QList<Recipe> dessertPage = RecipeDAO().browse(
        QString(), QStringLiteral("dessert"), userId, 0, 12, &browseTotal);
    expect(browseTotal > 0 && !dessertPage.isEmpty(), "食谱大全可查询甜品分类");
    bool dessertNamesValid = true;
    for (const Recipe &item : dessertPage) {
        dessertNamesValid = dessertNamesValid
            && (item.dishRole == QStringLiteral("dessert")
                || item.name.contains(QStringLiteral("蛋糕"))
                || item.name.contains(QStringLiteral("甜品"))
                || item.name.contains(QStringLiteral("糖水"))
                || item.name.contains(QStringLiteral("布丁"))
                || item.name.contains(QStringLiteral("酸奶杯"))
                || item.name.contains(QStringLiteral("甜羹")));
    }
    expect(dessertNamesValid, "食谱大全甜品分类不会混入普通菜品");
    std::fprintf(stderr, "feature-pack: catalog-ready\n");
    std::fflush(stderr);

    const IngredientMeasureEstimate waterBamboo =
        IngredientMeasureService::parse(QStringLiteral("茭白 4根"));
    expect(waterBamboo.valid && qAbs(waterBamboo.grams - 400.0) < 0.01
               && waterBamboo.quantityText == QStringLiteral("4根约400g"),
           "茭白可按每根平均重量换算并保留自然单位");
    const IngredientMeasureEstimate quantityFirst =
        IngredientMeasureService::parse(QStringLiteral("一根茭白"));
    expect(quantityFirst.valid && quantityFirst.ingredientName == QStringLiteral("茭白")
               && quantityFirst.quantityText == QStringLiteral("一根约100g"),
           "一根茭白等数量在前的写法也可换算");
    const IngredientMeasureEstimate teaspoon =
        IngredientMeasureService::parse(QStringLiteral("白糖 一茶勺"));
    expect(teaspoon.valid && qAbs(teaspoon.grams - 4.0) < 0.01
               && teaspoon.quantityText == QStringLiteral("一茶勺约4g"),
           "茶勺会结合食材密度估算克重并保留原单位");
    const Recipe repairedRecipe = RecipeDAO().findById(133);
    bool malformedIngredient = false;
    QStringList repairedNames;
    for (const RecipeIngredient &ingredient : repairedRecipe.ingredients) {
        malformedIngredient = malformedIngredient || ingredient.foodName.size() > 20;
        repairedNames.append(ingredient.foodName);
    }
    expect(!malformedIngredient && repairedNames.contains(QStringLiteral("肉蟹"))
               && repairedNames.contains(QStringLiteral("猪二刀肉"))
               && repairedNames.contains(QStringLiteral("洋葱")),
           "异常合并原料已拆分，主料完整保留");

    const Recipe toonCake = RecipeDAO().findById(441);
    QStringList toonIngredients;
    for (const RecipeIngredient &ingredient : toonCake.ingredients)
        toonIngredients.append(ingredient.foodName);
    expect(toonCake.ingredients.size() == 8
               && toonIngredients.contains(QStringLiteral("香椿芽"))
               && toonIngredients.contains(QStringLiteral("面粉"))
               && toonIngredients.contains(QStringLiteral("鸡蛋"))
               && toonCake.totalCalories > 300.0 && toonCake.totalCalories < 330.0,
           "煎香椿饼按十份折算补回香椿芽、面粉和完整原料并重算营养");

    PersonalRecipeDraft draft;
    draft.name = QStringLiteral("__功能测试_双鸡肉香料__");
    draft.category = QStringLiteral("晚餐");
    draft.steps = QStringLiteral("1. 鸡腿和鸡胸切块。\n2. 加八角焖熟，盐调味。");
    draft.sourceType = QStringLiteral("manual");
    draft.ingredients = {
        {QStringLiteral("鸡腿"), 100.0}, {QStringLiteral("鸡胸肉"), 100.0},
        {QStringLiteral("八角"), 2.0}, {QStringLiteral("盐"), 2.0},
    };
    QString error;
    const int recipeId = PersonalRecipeDAO().create(userId, draft, &error);
    expect(recipeId > 0, "手动食谱可保存");
    if (recipeId <= 0)
        qCritical() << error;
    const Recipe recipe = RecipeDAO().findById(recipeId);
    expect(recipe.ingredients.size() == 4, "两项主料、香料和调味料均独立保留");
    QStringList names;
    for (const RecipeIngredient &ingredient : recipe.ingredients)
        names.append(ingredient.foodName);
    expect(names.contains(QStringLiteral("鸡腿")) && names.contains(QStringLiteral("鸡胸肉")),
           "相似营养食材不会互相覆盖主料");
    expect(names.contains(QStringLiteral("八角")) && names.contains(QStringLiteral("盐")),
           "香料与调味料在原料清单中可追溯");

    PersonalRecipeDraft measuredDraft;
    measuredDraft.name = QStringLiteral("__功能测试_自然单位__");
    measuredDraft.category = QStringLiteral("晚餐");
    measuredDraft.dishRole = QStringLiteral("vegetable");
    measuredDraft.steps = QStringLiteral("1. 茭白切片炒熟。");
    measuredDraft.sourceType = QStringLiteral("manual");
    measuredDraft.ingredients = {
        {QStringLiteral("茭白"), waterBamboo.grams, waterBamboo.quantityText},
    };
    const int measuredRecipeId = PersonalRecipeDAO().create(userId, measuredDraft, &error);
    expect(measuredRecipeId > 0, "自然单位食谱可按换算克重保存");
    const Recipe measuredRecipe = RecipeDAO().findById(measuredRecipeId);
    expect(measuredRecipe.ingredients.size() == 1
               && measuredRecipe.ingredients.first().quantityText == QStringLiteral("4根约400g")
               && qAbs(measuredRecipe.ingredients.first().quantity - 400.0) < 0.01,
           "自然单位展示文本与营养计算克重分别持久化");

    PersonalRecipeDraft dnaDraft = draft;
    dnaDraft.name = QStringLiteral("__功能测试_DNA轻脂版__");
    dnaDraft.sourceType = QStringLiteral("dna");
    dnaDraft.sourceUrl = QStringLiteral("recipe:1");
    dnaDraft.steps = QStringLiteral("1. 去除可见脂肪。\n2. 少油炖煮并保留原香料。" );
    const int dnaRecipeId = PersonalRecipeDAO().create(userId, dnaDraft, &error);
    expect(dnaRecipeId > 0, "DNA改造结果可保存为个人食谱");
    QSqlQuery dnaSource(DatabaseManager::getInstance().database());
    dnaSource.prepare(QStringLiteral(
        "SELECT source_type,source_url FROM user_recipe_library "
        "WHERE user_id=:user AND recipe_id=:recipe"));
    dnaSource.bindValue(QStringLiteral(":user"), userId);
    dnaSource.bindValue(QStringLiteral(":recipe"), dnaRecipeId);
    expect(dnaSource.exec() && dnaSource.next()
               && dnaSource.value(0).toString() == QStringLiteral("dna")
               && dnaSource.value(1).toString() == QStringLiteral("recipe:1"),
           "DNA食谱保留来源类型与母本ID");
    expect(PersonalRecipeDAO().isPersonal(userId, recipeId), "个人食谱按用户关联");
    expect(RecipeDAO().isFavorite(userId, recipeId), "新建食谱自动加入当前用户收藏");
    bool leakedToPublicPool = false;
    for (const Recipe &publicRecipe : RecipeDAO().findAll())
        leakedToPublicPool = leakedToPublicPool || publicRecipe.id == recipeId;
    expect(!leakedToPublicPool, "个人食谱不会进入其他用户的公共推荐池");
    std::fprintf(stderr, "feature-pack: personal-recipe-ready\n");
    std::fflush(stderr);

    FridgeDAO().clearUser(userId);
    expect(FridgeDAO().upsert(userId, QStringLiteral("奇亚籽"), 100.0,
                              QStringLiteral("g"), QStringLiteral("2026-09-08"))
               && FridgeDAO().upsert(userId, QStringLiteral("奇亚籽"), 200.0,
                                     QStringLiteral("g"), QStringLiteral("2026-09-08"))
               && FridgeDAO().upsert(userId, QStringLiteral("奇亚籽"), 50.0,
                                     QStringLiteral("g"), QStringLiteral("2026-09-10")),
           "同种食材可按保质期批次写入");
    const QList<FridgeItem> chiaBatches = FridgeDAO().listByUser(userId);
    bool sameExpiryAccumulated = false;
    bool otherExpirySeparated = false;
    for (const FridgeItem &item : chiaBatches) {
        if (item.foodName != QStringLiteral("奇亚籽"))
            continue;
        if (item.expiryDate == QStringLiteral("2026-09-08"))
            sameExpiryAccumulated = qAbs(item.quantity - 300.0) < 0.01;
        if (item.expiryDate == QStringLiteral("2026-09-10"))
            otherExpirySeparated = qAbs(item.quantity - 50.0) < 0.01;
    }
    expect(chiaBatches.size() == 2 && sameExpiryAccumulated && otherExpirySeparated,
           "同名同保质期数量累加，不同保质期分行保存");
    FridgeDAO().clearUser(userId);
    FridgeDAO().upsert(userId, QStringLiteral("鸡腿"), 50.0, QStringLiteral("g"), QString());
    RecommendResult plan;
    plan.valid = true;
    plan.dinner.mealLabel = QStringLiteral("晚餐");
    plan.dinner.dishes.append(recipe);
    const QList<ShoppingListItem> shopping = ShoppingListService().build(userId, plan, 1);
    QStringList shoppingNames;
    double chickenLegBuy = -1.0;
    for (const ShoppingListItem &item : shopping) {
        shoppingNames.append(item.name);
        if (item.name == QStringLiteral("鸡腿"))
            chickenLegBuy = item.buyGrams;
    }
    expect(!shoppingNames.contains(QStringLiteral("盐")), "购物清单排除常备盐");
    expect(shoppingNames.contains(QStringLiteral("八角")), "购物清单保留八角等香料");
    expect(chickenLegBuy > 49.0 && chickenLegBuy < 51.0, "购物清单正确扣除冰箱已有鸡腿");
    const QString shared = ShoppingListService::toShareText(shopping, QStringLiteral("今日方案"));
    expect(shared.contains(QStringLiteral("八角")) && !shared.contains(QStringLiteral("□ 盐")),
           "分享文本遵循调味料与香料规则");

    expect(ShoppingListService::normalizedIngredientName(QStringLiteral("姜")) == QStringLiteral("姜")
               && ShoppingListService::normalizedIngredientName(QStringLiteral("生姜")) == QStringLiteral("姜")
               && ShoppingListService::normalizedIngredientName(QStringLiteral("姜小块")) == QStringLiteral("姜"),
           "姜、生姜、姜小块统一为标准名称姜");
    expect(ShoppingListService::isCommonPantrySeasoning(QStringLiteral("清水")),
           "清水不会出现在购物清单");
    std::fprintf(stderr, "feature-pack: shopping-ready\n");
    std::fflush(stderr);

    const QList<ShoppingListItem> exportItems = {
        {QStringLiteral("姜"), QStringLiteral("蔬菜菌菇"), 10.0, 2.0, 8.0, false},
        {QStringLiteral("八角"), QStringLiteral("调味与香料"), 2.0, 0.0, 2.0, true},
    };
    const QList<QPair<ShoppingListExportService::Format, QString>> formats = {
        {ShoppingListExportService::Format::Pdf, QStringLiteral("pdf")},
        {ShoppingListExportService::Format::Word, QStringLiteral("docx")},
        {ShoppingListExportService::Format::Excel, QStringLiteral("xlsx")},
        {ShoppingListExportService::Format::Text, QStringLiteral("txt")},
    };
    for (const auto &format : formats) {
        std::fprintf(stderr, "feature-pack: export-%s\n", qPrintable(format.second));
        std::fflush(stderr);
        const QString path = temp.filePath(QStringLiteral("shopping.") + format.second);
        QString exportError;
        expect(ShoppingListExportService::exportList(path, format.first, exportItems,
                                                       QStringLiteral("今日方案"), &exportError),
               qPrintable(QStringLiteral("可导出 %1 格式").arg(format.second)));
        QFile exported(path);
        expect(exported.open(QIODevice::ReadOnly) && exported.size() > 20,
               qPrintable(QStringLiteral("%1 导出文件非空").arg(format.second)));
        const QByteArray signature = exported.read(4);
        if (format.second == QStringLiteral("pdf"))
            expect(signature == QByteArrayLiteral("%PDF"), "PDF 文件签名正确");
        else if (format.second == QStringLiteral("docx") || format.second == QStringLiteral("xlsx"))
            expect(signature.startsWith(QByteArrayLiteral("PK")), "Office OpenXML 文件签名正确");
    }

    User adaptiveUser;
    adaptiveUser.id = userId;
    adaptiveUser.goal = QStringLiteral("lose");
    adaptiveUser.calorieTarget = 2100;
    adaptiveUser.weight = 70.0;
    const QDate adaptiveToday(2026, 9, 1);
    QList<DailyFoodLogPoint> adaptiveFood;
    QList<HealthDailyRecord> adaptiveHealth;
    for (int i = 0; i < 14; ++i) {
        DailyFoodLogTotals totals;
        totals.count = 3;
        totals.calories = 2250;
        totals.protein = 105;
        adaptiveFood.append({adaptiveToday.addDays(i - 13), totals});
        HealthDailyRecord record;
        record.date = adaptiveToday.addDays(i - 13);
        record.steps = 8420;
        record.activeCalories = 430;
        record.sleepHours = 7.1;
        if (i == 0)
            record.weightKg = 70.4;
        if (i == 13)
            record.weightKg = 70.0;
        record.source = QStringLiteral("apple_health");
        adaptiveHealth.append(record);
    }
    const AdaptiveTargetResult adaptive = AdaptiveTargetService::calculate(
        adaptiveUser, adaptiveFood, adaptiveHealth, 14);
    expect(adaptive.enoughData && adaptive.effectiveTarget == 2250
               && adaptive.decision == QStringLiteral("维持有效方案"),
           "实际摄入2250且两周减重0.4kg时维持有效方案，不机械限制到2100");
    expect(adaptive.explanation.contains(QStringLiteral("2250"))
               && adaptive.explanation.contains(QStringLiteral("下降0.4kg")),
           "动态目标解释可追溯摄入和体重证据");

    QString healthError;
    const QList<HealthDailyRecord> healthConnect = HealthDataSyncService::parseHealthConnectJson(
        QByteArrayLiteral("[{\"type\":\"StepsRecord\",\"startTime\":\"2026-09-01T08:00:00+08:00\",\"count\":8420},"
                          "{\"type\":\"ActiveCaloriesBurnedRecord\",\"startTime\":\"2026-09-01T09:00:00+08:00\",\"energy\":{\"inKilocalories\":430}},"
                          "{\"type\":\"WeightRecord\",\"startTime\":\"2026-09-01T07:00:00+08:00\",\"weight\":{\"inKilograms\":70}},"
                          "{\"type\":\"SleepSessionRecord\",\"startTime\":\"2026-09-01T00:00:00+08:00\",\"endTime\":\"2026-09-01T07:06:00+08:00\"}]"),
        &healthError);
    expect(healthConnect.size() == 1 && healthConnect.first().steps == 8420
               && qAbs(healthConnect.first().activeCalories - 430.0) < 0.01
               && qAbs(healthConnect.first().weightKg - 70.0) < 0.01
               && qAbs(healthConnect.first().sleepHours - 7.1) < 0.01,
           "Health Connect 步数、活动、体重和睡眠按日期合并解析");

    FlavorFingerprintService flavorService;
    const FlavorFingerprint sanbuzhanFlavor = flavorService.estimate(
        QStringLiteral("三不粘"),
        {{QStringLiteral("白糖"), 125.0}, {QStringLiteral("湿淀粉"), 30.0},
         {QStringLiteral("鸡蛋黄"), 49.5}, {QStringLiteral("猪油"), 3.3}},
        QStringLiteral("原料搅匀后下锅翻炒至成形"), 3.3, 207.8);
    expect(sanbuzhanFlavor.sweet > 0.0
               && sanbuzhanFlavor.sour == 0.0
               && sanbuzhanFlavor.salty == 0.0
               && sanbuzhanFlavor.spicy == 0.0
               && sanbuzhanFlavor.umami == 0.0,
           "三不粘仅由实际原料产生甜味，不再附带酸咸辣鲜基准分");

    QSqlQuery oldFlavorCache(DatabaseManager::getInstance().database());
    oldFlavorCache.prepare(QStringLiteral(
        "INSERT INTO recipe_flavor_fingerprints("
        "recipe_id,sweet,sour,salty,spicy,umami,aroma,crispy,soft,source) "
        "VALUES(:recipe,8,5,16,3,18,18,8,22,'rule-v1') "
        "ON CONFLICT(recipe_id) DO UPDATE SET sweet=8,sour=5,salty=16,spicy=3,"
        "umami=18,aroma=18,crispy=8,soft=22,source='rule-v1'"));
    oldFlavorCache.bindValue(QStringLiteral(":recipe"), recipeId);
    expect(oldFlavorCache.exec(), "可构造旧版风味缓存用于迁移验证");
    const FlavorFingerprint migratedFlavor = flavorService.forRecipe(recipe);
    expect(migratedFlavor.sweet == 0.0 && migratedFlavor.sour == 0.0
               && migratedFlavor.spicy == 0.0 && migratedFlavor.crispy == 4.0,
           "旧版固定基准分缓存会按实际食材和步骤自动重算");
    QSqlQuery migratedFlavorSource(DatabaseManager::getInstance().database());
    migratedFlavorSource.prepare(QStringLiteral(
        "SELECT source FROM recipe_flavor_fingerprints WHERE recipe_id=:recipe"));
    migratedFlavorSource.bindValue(QStringLiteral(":recipe"), recipeId);
    expect(migratedFlavorSource.exec() && migratedFlavorSource.next()
               && migratedFlavorSource.value(0).toString()
                      == QStringLiteral("rule-v3-texture-baseline"),
           "重算后的风味缓存写入低口感基准规则版本");

    const FlavorFingerprint originalFlavor = flavorService.estimate(
        QStringLiteral("家常红烧肉"),
        {{QStringLiteral("五花肉"), 220.0}, {QStringLiteral("冰糖"), 15.0},
         {QStringLiteral("酱油"), 12.0}, {QStringLiteral("姜"), 8.0}},
        QStringLiteral("炒糖色后小火炖至软糯"), 52.0, 255.0);
    const FlavorFingerprint editedFlavor = flavorService.estimate(
        QStringLiteral("甜香轻脂红烧肉"),
        {{QStringLiteral("瘦肉"), 150.0}, {QStringLiteral("冰糖"), 24.0},
         {QStringLiteral("酱油"), 12.0}, {QStringLiteral("香菇"), 70.0},
         {QStringLiteral("姜"), 8.0}},
        QStringLiteral("少油炒香后小火炖至软糯"), 30.0, 264.0);
    expect(editedFlavor.sweet > originalFlavor.sweet
               && FlavorFingerprintService::similarity(originalFlavor, editedFlavor) >= 60,
           "风味指纹能反映甜度提高并量化与原版的相似度");
    expect(FlavorFingerprintService::comparisonSummary(originalFlavor, editedFlavor)
               .contains(QStringLiteral("风味相似度")),
           "风味指纹输出可读的相似度与变化摘要");

    std::fprintf(stderr, "feature-pack: complete\n");
    std::fflush(stderr);

    return failures == 0 ? 0 : 1;
}
