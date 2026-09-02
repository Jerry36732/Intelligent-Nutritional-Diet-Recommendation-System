#include "DatabaseManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QRegularExpression>
#include <QDebug>

DatabaseManager &DatabaseManager::getInstance()
{
    static DatabaseManager instance;
    return instance;
}

bool DatabaseManager::open(const QString &path)
{
    // 确保能找到 plugins/sqldrivers（Qt Creator 直接运行未部署时的兜底）
    const QString appDir = QCoreApplication::applicationDirPath();
    QCoreApplication::addLibraryPath(appDir);
    QCoreApplication::addLibraryPath(appDir + QStringLiteral("/plugins"));
    const QString qtPlugins = QDir(QCoreApplication::applicationDirPath())
                                  .absoluteFilePath(QStringLiteral("../plugins"));
    Q_UNUSED(qtPlugins);

    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        qWarning() << "DatabaseManager: QSQLITE driver not available."
                   << "Available:" << QSqlDatabase::drivers()
                   << "Library paths:" << QCoreApplication::libraryPaths();
        return false;
    }

    if (QSqlDatabase::contains(kConnectionName)) {
        {
            QSqlDatabase existing = QSqlDatabase::database(kConnectionName);
            if (existing.isOpen()) {
                if (existing.databaseName() == path)
                    return true;
                existing.close();
            }
        }
        QSqlDatabase::removeDatabase(kConnectionName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kConnectionName);
    if (!db.isValid()) {
        qWarning() << "DatabaseManager: invalid QSQLITE connection object";
        return false;
    }
    db.setDatabaseName(path);

    if (!db.open()) {
        qWarning() << "DatabaseManager: failed to open" << path << db.lastError().text();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(kConnectionName);
        return false;
    }

    QSqlQuery pragma(db);
    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));

    return ensureSchema();
}

QSqlDatabase DatabaseManager::database() const
{
    return QSqlDatabase::database(kConnectionName);
}

bool DatabaseManager::isOpen() const
{
    QSqlDatabase db = database();
    return db.isValid() && db.isOpen();
}

bool DatabaseManager::ensureSchema()
{
    QSqlDatabase db = database();
    if (!db.isOpen())
        return false;

    QSqlQuery q(db);

    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS users ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  name TEXT UNIQUE NOT NULL,"
            "  gender TEXT CHECK(gender IN ('male','female')) DEFAULT 'male',"
            "  goal TEXT CHECK(goal IN ('lose','gain','maintain')) DEFAULT 'gain',"
            "  height REAL DEFAULT 175,"
            "  weight REAL DEFAULT 70,"
            "  age INTEGER DEFAULT 25,"
            "  calorie_target INTEGER DEFAULT 2100,"
            "  password_hash TEXT"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS recipes ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  name TEXT NOT NULL,"
            "  category TEXT CHECK(category IN ('早餐','午餐','晚餐')) NOT NULL,"
            "  steps TEXT,"
            "  cook_minutes INTEGER DEFAULT 15,"
            "  accent TEXT DEFAULT 'green',"
            "  total_calories REAL,"
            "  total_protein REAL,"
            "  total_carbs REAL,"
            "  total_fat REAL"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS recipe_foods ("
            "  recipe_id INTEGER NOT NULL,"
            "  food_id INTEGER NOT NULL,"
            "  quantity REAL NOT NULL,"
            "  PRIMARY KEY (recipe_id, food_id),"
            "  FOREIGN KEY (recipe_id) REFERENCES recipes(id),"
            "  FOREIGN KEY (food_id) REFERENCES foods(id)"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS favorites ("
            "  user_id INTEGER NOT NULL,"
            "  recipe_id INTEGER NOT NULL,"
            "  created_at TEXT NOT NULL DEFAULT (datetime('now','localtime')),"
            "  PRIMARY KEY (user_id, recipe_id),"
            "  FOREIGN KEY (user_id) REFERENCES users(id),"
            "  FOREIGN KEY (recipe_id) REFERENCES recipes(id)"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS food_favorites ("
            "  user_id INTEGER NOT NULL,"
            "  food_id INTEGER NOT NULL,"
            "  created_at TEXT NOT NULL DEFAULT (datetime('now','localtime')),"
            "  PRIMARY KEY (user_id, food_id),"
            "  FOREIGN KEY (user_id) REFERENCES users(id),"
            "  FOREIGN KEY (food_id) REFERENCES foods(id)"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS user_favorites ("
            "  user_id INTEGER NOT NULL,"
            "  item_type TEXT NOT NULL CHECK(item_type IN ('recipe','ingredient')),"
            "  item_id INTEGER NOT NULL,"
            "  created_at TEXT NOT NULL DEFAULT (datetime('now','localtime')),"
            "  PRIMARY KEY (user_id, item_type, item_id),"
            "  FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS user_recipe_library ("
            "  user_id INTEGER NOT NULL,"
            "  recipe_id INTEGER NOT NULL,"
            "  source_type TEXT NOT NULL DEFAULT 'manual',"
            "  source_url TEXT,"
            "  created_at TEXT NOT NULL DEFAULT (datetime('now','localtime')),"
            "  PRIMARY KEY (user_id, recipe_id),"
            "  FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,"
            "  FOREIGN KEY (recipe_id) REFERENCES recipes(id) ON DELETE CASCADE"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS user_food_logs ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  user_id INTEGER NOT NULL,"
            "  eaten_at TEXT NOT NULL DEFAULT (datetime('now','localtime')),"
            "  meal_label TEXT NOT NULL DEFAULT '加餐',"
            "  food_name TEXT NOT NULL,"
            "  serving_grams REAL NOT NULL,"
            "  calories REAL NOT NULL,"
            "  protein REAL NOT NULL DEFAULT 0,"
            "  carbs REAL NOT NULL DEFAULT 0,"
            "  fat REAL NOT NULL DEFAULT 0,"
            "  confidence REAL NOT NULL DEFAULT 0,"
            "  provider TEXT,"
            "  image_path TEXT,"
            "  notes TEXT,"
            "  created_at TEXT NOT NULL DEFAULT (datetime('now','localtime')),"
            "  FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE"
            ")"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_user_food_logs_day "
            "ON user_food_logs(user_id,eaten_at)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS user_health_daily ("
            "  user_id INTEGER NOT NULL,"
            "  record_date TEXT NOT NULL,"
            "  steps INTEGER NOT NULL DEFAULT 0,"
            "  active_calories REAL NOT NULL DEFAULT 0,"
            "  weight_kg REAL NOT NULL DEFAULT 0,"
            "  sleep_hours REAL NOT NULL DEFAULT 0,"
            "  source TEXT NOT NULL,"
            "  updated_at TEXT NOT NULL DEFAULT (datetime('now','localtime')),"
            "  PRIMARY KEY (user_id,record_date,source),"
            "  FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE"
            ")"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_user_health_daily_range "
            "ON user_health_daily(user_id,record_date)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS health_sync_sources ("
            "  user_id INTEGER NOT NULL,"
            "  platform TEXT NOT NULL,"
            "  display_name TEXT NOT NULL,"
            "  last_synced_at TEXT,"
            "  from_date TEXT,"
            "  to_date TEXT,"
            "  record_count INTEGER NOT NULL DEFAULT 0,"
            "  status TEXT NOT NULL DEFAULT 'ready',"
            "  PRIMARY KEY (user_id,platform),"
            "  FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS recipe_flavor_fingerprints ("
            "  recipe_id INTEGER PRIMARY KEY,"
            "  sweet REAL NOT NULL DEFAULT 0,"
            "  sour REAL NOT NULL DEFAULT 0,"
            "  salty REAL NOT NULL DEFAULT 0,"
            "  spicy REAL NOT NULL DEFAULT 0,"
            "  umami REAL NOT NULL DEFAULT 0,"
            "  aroma REAL NOT NULL DEFAULT 0,"
            "  crispy REAL NOT NULL DEFAULT 0,"
            "  soft REAL NOT NULL DEFAULT 0,"
            "  source TEXT NOT NULL DEFAULT 'rule-v1',"
            "  updated_at TEXT NOT NULL DEFAULT (datetime('now','localtime')),"
            "  FOREIGN KEY (recipe_id) REFERENCES recipes(id) ON DELETE CASCADE"
            ")"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_recipe_category ON recipes(category)")
    };

    for (const QString &sql : statements) {
        if (!q.exec(sql)) {
            qWarning() << "DatabaseManager::ensureSchema failed:" << q.lastError().text()
                       << "SQL:" << sql;
            return false;
        }
    }

    // 轻量迁移：旧库补 password_hash / category_label
    auto ensureColumn = [&](const QString &table, const QString &column, const QString &type) {
        QSqlQuery info(db);
        info.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table));
        bool found = false;
        while (info.next()) {
            if (info.value(1).toString() == column) {
                found = true;
                break;
            }
        }
        if (!found) {
            QSqlQuery alter(db);
            if (!alter.exec(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3")
                                .arg(table, column, type))) {
                qWarning() << "migrate column failed:" << alter.lastError().text();
            }
        }
    };
    ensureColumn(QStringLiteral("users"), QStringLiteral("password_hash"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("users"), QStringLiteral("age"), QStringLiteral("INTEGER DEFAULT 25"));
    ensureColumn(QStringLiteral("users"), QStringLiteral("preferences"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("users"), QStringLiteral("allergens"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("users"), QStringLiteral("dietary_choices"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("users"), QStringLiteral("food_intolerances"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("users"), QStringLiteral("nutritional_deficiencies"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("users"), QStringLiteral("allergies"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("users"), QStringLiteral("medical_conditions"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("health_sync_sources"), QStringLiteral("from_date"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("health_sync_sources"), QStringLiteral("to_date"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("health_sync_sources"), QStringLiteral("last_synced_at"), QStringLiteral("TEXT"));
    // SQLite 对已有表 ADD COLUMN 时不接受 CURRENT_TIMESTAMP 这类非常量默认值，
    // 因此先兼容补列，再为旧记录回填；新记录由 DAO 明确写入 created_at。
    ensureColumn(QStringLiteral("favorites"), QStringLiteral("created_at"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("food_favorites"), QStringLiteral("created_at"), QStringLiteral("TEXT"));
    q.exec(QStringLiteral("UPDATE favorites SET created_at=datetime('now','localtime') "
                          "WHERE created_at IS NULL OR created_at=''"));
    q.exec(QStringLiteral("UPDATE food_favorites SET created_at=datetime('now','localtime') "
                          "WHERE created_at IS NULL OR created_at=''"));
    // 统一收藏数据源。旧版双表保留并迁移，避免清空已有用户收藏。
    q.exec(QStringLiteral(
        "INSERT OR IGNORE INTO user_favorites(user_id,item_type,item_id,created_at) "
        "SELECT user_id,'recipe',recipe_id,COALESCE(NULLIF(created_at,''),datetime('now','localtime')) "
        "FROM favorites"));
    q.exec(QStringLiteral(
        "INSERT OR IGNORE INTO user_favorites(user_id,item_type,item_id,created_at) "
        "SELECT user_id,'ingredient',food_id,COALESCE(NULLIF(created_at,''),datetime('now','localtime')) "
        "FROM food_favorites"));
    ensureColumn(QStringLiteral("foods"), QStringLiteral("category_label"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("recipes"), QStringLiteral("dish_role"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("recipes"), QStringLiteral("source"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("recipes"), QStringLiteral("source_url"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("recipes"), QStringLiteral("total_weight"), QStringLiteral("REAL"));
    ensureColumn(QStringLiteral("recipes"), QStringLiteral("per100_calories"), QStringLiteral("REAL"));
    ensureColumn(QStringLiteral("recipes"), QStringLiteral("per100_protein"), QStringLiteral("REAL"));
    ensureColumn(QStringLiteral("recipes"), QStringLiteral("per100_fat"), QStringLiteral("REAL"));
    ensureColumn(QStringLiteral("recipes"), QStringLiteral("per100_carbs"), QStringLiteral("REAL"));
    ensureColumn(QStringLiteral("recipes"), QStringLiteral("source_ref"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("recipes"), QStringLiteral("nutrition_verified_at"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("recipe_foods"), QStringLiteral("display_name"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("recipe_foods"), QStringLiteral("source_text"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("recipe_foods"), QStringLiteral("quantity_text"), QStringLiteral("TEXT"));

    // MDB:4826 的旧导入记录丢失了数字，错误地只保留了 100g 鸡蛋。
    // 原始配方可制作 10 份，这里按每份折算并幂等修复原料与营养。
    {
        QSqlQuery recipeLookup(db);
        recipeLookup.prepare(QStringLiteral(
            "SELECT id FROM recipes WHERE source_ref='MDB:4826' OR name='煎香椿饼' "
            "ORDER BY CASE WHEN source_ref='MDB:4826' THEN 0 ELSE 1 END LIMIT 1"));
        int recipeId = 0;
        if (recipeLookup.exec() && recipeLookup.next())
            recipeId = recipeLookup.value(0).toInt();
        recipeLookup.finish();

        bool needsRepair = false;
        if (recipeId > 0) {
            QSqlQuery check(db);
            check.prepare(QStringLiteral(
                "SELECT COUNT(*), SUM(CASE WHEN COALESCE(NULLIF(rf.display_name,''),f.name) "
                "LIKE '%香椿%' THEN 1 ELSE 0 END) "
                "FROM recipe_foods rf JOIN foods f ON f.id=rf.food_id WHERE rf.recipe_id=:rid"));
            check.bindValue(QStringLiteral(":rid"), recipeId);
            if (check.exec() && check.next())
                needsRepair = check.value(0).toInt() < 8 || check.value(1).toInt() == 0;
            check.finish();
        }

        if (needsRepair) {
            struct IngredientRepair {
                int foodId;
                QString displayName;
                double grams;
                QString quantityText;
                QString sourceText;
            };
            const QList<IngredientRepair> ingredients = {
                {1, QStringLiteral("面粉"), 50.0, QStringLiteral("50 g"),
                 QStringLiteral("原配方：面粉500克；按10份折算")},
                {304, QStringLiteral("香椿芽"), 25.0, QStringLiteral("25 g"),
                 QStringLiteral("原配方：香椿芽250克；按10份折算")},
                {835, QStringLiteral("鸡蛋"), 20.0, QStringLiteral("20 g（原配方4个/10份）"),
                 QStringLiteral("原配方：鸡蛋4个；按10份折算")},
                {185, QStringLiteral("葱花"), 1.0, QStringLiteral("1 g"),
                 QStringLiteral("原配方：葱花10克；按10份折算")},
                {2069, QStringLiteral("精盐"), 0.5, QStringLiteral("0.5 g"),
                 QStringLiteral("原配方：精盐5克；按10份折算")},
                {2070, QStringLiteral("味精"), 0.2, QStringLiteral("0.2 g"),
                 QStringLiteral("原配方：味精2克；按10份折算")},
                {1943, QStringLiteral("香油"), 0.5, QStringLiteral("0.5 g"),
                 QStringLiteral("原配方：香油5克；按10份折算")},
                {1913, QStringLiteral("化猪油"), 10.0, QStringLiteral("10 g"),
                 QStringLiteral("原配方：化猪油100克；按10份折算")},
            };

            q.finish();
            bool repaired = db.transaction();
            if (repaired) {
                QSqlQuery removeOld(db);
                removeOld.prepare(QStringLiteral("DELETE FROM recipe_foods WHERE recipe_id=:rid"));
                removeOld.bindValue(QStringLiteral(":rid"), recipeId);
                repaired = removeOld.exec();
            }
            for (const IngredientRepair &ingredient : ingredients) {
                if (!repaired)
                    break;
                QSqlQuery insert(db);
                insert.prepare(QStringLiteral(
                    "INSERT INTO recipe_foods(recipe_id,food_id,quantity,display_name,source_text,quantity_text) "
                    "SELECT :rid,id,:grams,:display,:source,:quantityText FROM foods WHERE id=:foodId"));
                insert.bindValue(QStringLiteral(":rid"), recipeId);
                insert.bindValue(QStringLiteral(":foodId"), ingredient.foodId);
                insert.bindValue(QStringLiteral(":grams"), ingredient.grams);
                insert.bindValue(QStringLiteral(":display"), ingredient.displayName);
                insert.bindValue(QStringLiteral(":source"), ingredient.sourceText);
                insert.bindValue(QStringLiteral(":quantityText"), ingredient.quantityText);
                repaired = insert.exec() && insert.numRowsAffected() == 1;
                if (!repaired)
                    qWarning() << "repair 煎香椿饼 ingredient:" << insert.lastError().text();
            }

            if (repaired) {
                QSqlQuery nutrition(db);
                nutrition.prepare(QStringLiteral(
                    "SELECT COALESCE(SUM(rf.quantity),0),"
                    "COALESCE(SUM(COALESCE(f.calories,0)*rf.quantity/100.0),0),"
                    "COALESCE(SUM(COALESCE(f.protein,0)*rf.quantity/100.0),0),"
                    "COALESCE(SUM(COALESCE(f.fat,0)*rf.quantity/100.0),0),"
                    "COALESCE(SUM(COALESCE(f.carbs,0)*rf.quantity/100.0),0) "
                    "FROM recipe_foods rf JOIN foods f ON f.id=rf.food_id WHERE rf.recipe_id=:rid"));
                nutrition.bindValue(QStringLiteral(":rid"), recipeId);
                repaired = nutrition.exec() && nutrition.next();
                if (repaired) {
                    const double weight = nutrition.value(0).toDouble();
                    const double calories = nutrition.value(1).toDouble();
                    const double protein = nutrition.value(2).toDouble();
                    const double fat = nutrition.value(3).toDouble();
                    const double carbs = nutrition.value(4).toDouble();
                    QSqlQuery update(db);
                    update.prepare(QStringLiteral(
                        "UPDATE recipes SET total_weight=:weight,total_calories=:calories,"
                        "total_protein=:protein,total_fat=:fat,total_carbs=:carbs,"
                        "per100_calories=:pc,per100_protein=:pp,per100_fat=:pf,per100_carbs=:pcb,"
                        "nutrition_verified_at=datetime('now','localtime') WHERE id=:rid"));
                    update.bindValue(QStringLiteral(":weight"), weight);
                    update.bindValue(QStringLiteral(":calories"), calories);
                    update.bindValue(QStringLiteral(":protein"), protein);
                    update.bindValue(QStringLiteral(":fat"), fat);
                    update.bindValue(QStringLiteral(":carbs"), carbs);
                    update.bindValue(QStringLiteral(":pc"), weight > 0 ? calories * 100.0 / weight : 0.0);
                    update.bindValue(QStringLiteral(":pp"), weight > 0 ? protein * 100.0 / weight : 0.0);
                    update.bindValue(QStringLiteral(":pf"), weight > 0 ? fat * 100.0 / weight : 0.0);
                    update.bindValue(QStringLiteral(":pcb"), weight > 0 ? carbs * 100.0 / weight : 0.0);
                    update.bindValue(QStringLiteral(":rid"), recipeId);
                    repaired = update.exec();
                }
            }
            if (repaired) {
                if (!db.commit())
                    qWarning() << "commit 煎香椿饼 repair:" << db.lastError().text();
            } else {
                db.rollback();
                qWarning() << "煎香椿饼 ingredient repair rolled back";
            }
        }
    }

    // 修复旧版 MDB 导入器产生的重复步骤编号，例如“1. 1。步骤正文”。
    // 使用幂等 replace，既兼容现有用户数据库，也不会改动已经规范的数据。
    for (int stepNumber = 1; stepNumber <= 30; ++stepNumber) {
        const QString duplicated = QStringLiteral("%1. %1。").arg(stepNumber);
        const QString normalized = QStringLiteral("%1. ").arg(stepNumber);
        QSqlQuery fixRecipeSteps(db);
        fixRecipeSteps.prepare(QStringLiteral(
            "UPDATE recipes SET steps=replace(steps,:duplicated,:normalized) "
            "WHERE instr(steps,:duplicated)>0"));
        fixRecipeSteps.bindValue(QStringLiteral(":duplicated"), duplicated);
        fixRecipeSteps.bindValue(QStringLiteral(":normalized"), normalized);
        if (!fixRecipeSteps.exec())
            qWarning() << "fix duplicated recipe step numbers:"
                       << fixRecipeSteps.lastError().text();
    }

    // RDSS 知识库表
    const QStringList knowledgeTables = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS allergen_food_map ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  allergen TEXT NOT NULL,"
            "  food_keyword TEXT NOT NULL,"
            "  UNIQUE(allergen, food_keyword)"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS nutrient_food_map ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  nutrient TEXT NOT NULL,"
            "  food_keyword TEXT NOT NULL,"
            "  boost_score REAL NOT NULL DEFAULT 1.0,"
            "  UNIQUE(nutrient, food_keyword)"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS deficiency_nutrient_map ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  deficiency TEXT NOT NULL UNIQUE,"
            "  nutrient TEXT NOT NULL,"
            "  description TEXT"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS medical_food_restrict ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  condition_name TEXT NOT NULL,"
            "  ban_keyword TEXT NOT NULL,"
            "  UNIQUE(condition_name, ban_keyword)"
            ")"),
    };
    for (const QString &sql : knowledgeTables) {
        if (!q.exec(sql))
            qWarning() << "knowledge table:" << q.lastError().text();
    }

    auto seedPair = [&](const QString &sql, const QString &a, const QString &b) {
        QSqlQuery ins(db);
        ins.prepare(sql);
        ins.bindValue(QStringLiteral(":a"), a);
        ins.bindValue(QStringLiteral(":b"), b);
        ins.exec();
    };

    // 仅在空表时灌入种子，避免重复
    QSqlQuery cnt(db);
    cnt.exec(QStringLiteral("SELECT COUNT(*) FROM allergen_food_map"));
    if (cnt.next() && cnt.value(0).toInt() == 0) {
        const auto allergenMap = QList<QPair<QString, QString>>{
            {QStringLiteral("豆制品"), QStringLiteral("豆腐")},
            {QStringLiteral("豆制品"), QStringLiteral("豆浆")},
            {QStringLiteral("豆制品"), QStringLiteral("豆皮")},
            {QStringLiteral("豆制品"), QStringLiteral("腐竹")},
            {QStringLiteral("豆制品"), QStringLiteral("豆芽")},
            {QStringLiteral("豆制品"), QStringLiteral("豆腐干")},
            {QStringLiteral("豆制品"), QStringLiteral("黄豆")},
            {QStringLiteral("花生"), QStringLiteral("花生")},
            {QStringLiteral("牛奶"), QStringLiteral("牛奶")},
            {QStringLiteral("海鲜"), QStringLiteral("虾")},
            {QStringLiteral("海鲜"), QStringLiteral("蟹")},
            {QStringLiteral("海鲜"), QStringLiteral("鱼")},
        };
        for (const auto &p : allergenMap)
            seedPair(QStringLiteral(
                         "INSERT OR IGNORE INTO allergen_food_map(allergen, food_keyword) "
                         "VALUES(:a,:b)"),
                     p.first, p.second);
    }

    cnt.exec(QStringLiteral("SELECT COUNT(*) FROM nutrient_food_map"));
    if (cnt.next() && cnt.value(0).toInt() == 0) {
        const auto nutrientMap = QList<QPair<QString, QString>>{
            {QStringLiteral("铁"), QStringLiteral("菠菜")},
            {QStringLiteral("铁"), QStringLiteral("猪肝")},
            {QStringLiteral("铁"), QStringLiteral("牛肉")},
            {QStringLiteral("钙"), QStringLiteral("牛奶")},
            {QStringLiteral("钙"), QStringLiteral("豆腐")},
            {QStringLiteral("蛋白质"), QStringLiteral("鸡胸")},
            {QStringLiteral("蛋白质"), QStringLiteral("鸡蛋")},
        };
        for (const auto &p : nutrientMap)
            seedPair(QStringLiteral(
                         "INSERT OR IGNORE INTO nutrient_food_map(nutrient, food_keyword) "
                         "VALUES(:a,:b)"),
                     p.first, p.second);
    }

    cnt.exec(QStringLiteral("SELECT COUNT(*) FROM deficiency_nutrient_map"));
    if (cnt.next() && cnt.value(0).toInt() == 0) {
        QSqlQuery ins(db);
        ins.exec(QStringLiteral(
            "INSERT OR IGNORE INTO deficiency_nutrient_map(deficiency, nutrient, description) VALUES "
            "('缺铁','铁','需增加铁摄入'),"
            "('缺钙','钙','需增加钙摄入'),"
            "('缺维生素D','维生素D','需增加维生素D'),"
            "('缺维生素B12','维生素B12','需增加B12'),"
            "('蛋白质不足','蛋白质','需增加优质蛋白')"));
    }

    cnt.exec(QStringLiteral("SELECT COUNT(*) FROM medical_food_restrict"));
    if (cnt.next() && cnt.value(0).toInt() == 0) {
        const auto medMap = QList<QPair<QString, QString>>{
            {QStringLiteral("2型糖尿病"), QStringLiteral("甜品")},
            {QStringLiteral("2型糖尿病"), QStringLiteral("蛋糕")},
            {QStringLiteral("高血压"), QStringLiteral("腌")},
            {QStringLiteral("高血脂"), QStringLiteral("肥肉")},
        };
        for (const auto &p : medMap)
            seedPair(QStringLiteral(
                         "INSERT OR IGNORE INTO medical_food_restrict(condition_name, ban_keyword) "
                         "VALUES(:a,:b)"),
                     p.first, p.second);
    }

    // ---- NAct 结构化知识库 + 推荐历史 ----
    const QStringList nactTables = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS food_nutrient_relations ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  food_id INTEGER NOT NULL,"
            "  nutrient_id TEXT NOT NULL,"
            "  concentration_level TEXT NOT NULL DEFAULT '中',"
            "  UNIQUE(food_id, nutrient_id))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS disease_nutrient_rules ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  disease_name TEXT NOT NULL,"
            "  nutrient_id TEXT NOT NULL,"
            "  rule_type TEXT NOT NULL,"
            "  description TEXT,"
            "  UNIQUE(disease_name, nutrient_id, rule_type))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS allergen_food_mapping ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  allergen_name TEXT NOT NULL,"
            "  food_id INTEGER NOT NULL,"
            "  UNIQUE(allergen_name, food_id))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS nutrient_deficiency_foods ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  deficiency_name TEXT NOT NULL,"
            "  food_id INTEGER NOT NULL,"
            "  priority INTEGER NOT NULL DEFAULT 3,"
            "  UNIQUE(deficiency_name, food_id))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS user_recommendation_history ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  user_id INTEGER NOT NULL,"
            "  recipe_id INTEGER NOT NULL,"
            "  recipe_name TEXT,"
            "  meal_label TEXT,"
            "  recommended_on TEXT NOT NULL DEFAULT (date('now','localtime')))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS fridge_inventory ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  user_id INTEGER NOT NULL,"
            "  food_name TEXT NOT NULL,"
            "  quantity REAL NOT NULL DEFAULT 1,"
            "  unit TEXT,"
            "  expiry_date TEXT,"
            "  updated_at TEXT NOT NULL DEFAULT (datetime('now','localtime')))"),
    };
    for (const QString &sql : nactTables) {
        if (!q.exec(sql))
            qWarning() << "nact table:" << q.lastError().text();
    }
    ensureColumn(QStringLiteral("fridge_inventory"), QStringLiteral("expiry_date"),
                 QStringLiteral("TEXT"));

    // 旧版只允许每位用户的同名食材存在一行，会把不同保质期的批次互相覆盖。
    // SQLite 不能直接删除 UNIQUE 约束，因此只在检测到旧结构时重建该表。
    QSqlQuery fridgeSchema(db);
    fridgeSchema.prepare(QStringLiteral(
        "SELECT sql FROM sqlite_master WHERE type='table' AND name='fridge_inventory'"));
    QString fridgeTableSql;
    if (fridgeSchema.exec() && fridgeSchema.next())
        fridgeTableSql = fridgeSchema.value(0).toString();
    fridgeSchema.finish();
    QString compactSchema = fridgeTableSql.toLower();
    compactSchema.remove(QRegularExpression(QStringLiteral("\\s+")));
    bool hasLegacyUnique = compactSchema.contains(QStringLiteral("unique(user_id,food_name)"));
    if (!hasLegacyUnique) {
        QSqlQuery indexes(db);
        if (indexes.exec(QStringLiteral("PRAGMA index_list(fridge_inventory)"))) {
            while (indexes.next() && !hasLegacyUnique) {
                if (!indexes.value(2).toBool())
                    continue;
                QString indexName = indexes.value(1).toString();
                indexName.replace(QLatin1Char('\''), QStringLiteral("''"));
                QSqlQuery columns(db);
                QStringList names;
                if (columns.exec(QStringLiteral("PRAGMA index_info('%1')").arg(indexName))) {
                    while (columns.next())
                        names.append(columns.value(2).toString().toLower());
                }
                columns.finish();
                hasLegacyUnique = names.size() == 2
                                  && names.contains(QStringLiteral("user_id"))
                                  && names.contains(QStringLiteral("food_name"));
            }
        }
        indexes.finish();
    }
    if (hasLegacyUnique) {
        // 结束所有仍占用 sqlite_master / 旧表的查询，避免 Windows 下 ALTER TABLE
        // 因 schema lock 失败后静默保留旧 UNIQUE 约束。
        q.finish();
        cnt.finish();
        if (!db.transaction()) {
            qWarning() << "fridge schema migration: cannot start transaction" << db.lastError().text();
        } else {
            QSqlQuery migrate(db);
            const QStringList migration = {
                QStringLiteral("ALTER TABLE fridge_inventory RENAME TO fridge_inventory_legacy"),
                QStringLiteral(
                    "CREATE TABLE fridge_inventory ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL, "
                    "food_name TEXT NOT NULL, quantity REAL NOT NULL DEFAULT 1, unit TEXT, "
                    "expiry_date TEXT, updated_at TEXT NOT NULL DEFAULT (datetime('now','localtime')))"),
                QStringLiteral(
                    "INSERT INTO fridge_inventory(id,user_id,food_name,quantity,unit,expiry_date,updated_at) "
                    "SELECT id,user_id,food_name,quantity,IFNULL(unit,''),IFNULL(expiry_date,''),updated_at "
                    "FROM fridge_inventory_legacy"),
                QStringLiteral("DROP TABLE fridge_inventory_legacy"),
            };
            bool migrated = true;
            for (const QString &sql : migration) {
                if (!migrate.exec(sql)) {
                    qWarning() << "fridge schema migration:" << migrate.lastError().text();
                    migrated = false;
                    break;
                }
            }
            if (migrated) {
                if (!db.commit())
                    qWarning() << "fridge schema migration commit:" << db.lastError().text();
            } else {
                db.rollback();
            }
        }
    }
    if (!q.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_fridge_inventory_batch "
            "ON fridge_inventory(user_id, food_name, expiry_date, unit)")))
        qWarning() << "fridge batch index:" << q.lastError().text();

    auto seedFoodNutrientByName = [&](const QString &nutrient, const QStringList &nameLikes,
                                      const QString &level) {
        for (const QString &like : nameLikes) {
            QSqlQuery ins(db);
            ins.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO food_nutrient_relations(food_id, nutrient_id, concentration_level) "
                "SELECT id, :n, :lv FROM foods WHERE name LIKE :like LIMIT 40"));
            ins.bindValue(QStringLiteral(":n"), nutrient);
            ins.bindValue(QStringLiteral(":lv"), level);
            ins.bindValue(QStringLiteral(":like"), QStringLiteral("%") + like + QStringLiteral("%"));
            ins.exec();
        }
    };

    cnt.exec(QStringLiteral("SELECT COUNT(*) FROM food_nutrient_relations"));
    if (cnt.next() && cnt.value(0).toInt() == 0) {
        seedFoodNutrientByName(QStringLiteral("铁"),
                               {QStringLiteral("菠菜"), QStringLiteral("猪肝"), QStringLiteral("牛肉"),
                                QStringLiteral("鸭血"), QStringLiteral("木耳")},
                               QStringLiteral("高"));
        seedFoodNutrientByName(QStringLiteral("钙"),
                               {QStringLiteral("牛奶"), QStringLiteral("酸奶"), QStringLiteral("豆腐"),
                                QStringLiteral("芝麻"), QStringLiteral("虾皮")},
                               QStringLiteral("高"));
        seedFoodNutrientByName(QStringLiteral("蛋白质"),
                               {QStringLiteral("鸡"), QStringLiteral("蛋"), QStringLiteral("鱼"),
                                QStringLiteral("虾"), QStringLiteral("牛肉"), QStringLiteral("瘦肉")},
                               QStringLiteral("高"));
        seedFoodNutrientByName(QStringLiteral("维生素D"),
                               {QStringLiteral("蛋黄"), QStringLiteral("蘑菇"), QStringLiteral("鱼")},
                               QStringLiteral("中"));
        seedFoodNutrientByName(QStringLiteral("维生素B12"),
                               {QStringLiteral("猪肝"), QStringLiteral("牛肉"), QStringLiteral("鸡蛋")},
                               QStringLiteral("高"));
    }

    cnt.exec(QStringLiteral("SELECT COUNT(*) FROM disease_nutrient_rules"));
    if (cnt.next() && cnt.value(0).toInt() == 0) {
        q.exec(QStringLiteral(
            "INSERT OR IGNORE INTO disease_nutrient_rules(disease_name, nutrient_id, rule_type, description) VALUES "
            "('2型糖尿病','糖','限制','甜品，蛋糕，糖水，奶茶，红糖'),"
            "('高血压','钠','限制','腌，咸鱼，腊肉，香肠'),"
            "('高血脂','脂肪','限制','肥肉，油炸，五花，奶油'),"
            "('贫血','铁','促进','菠菜，猪肝，瘦红肉'),"
            "('缺铁','铁','促进','菠菜，动物肝脏')"));
    }

    cnt.exec(QStringLiteral("SELECT COUNT(*) FROM allergen_food_mapping"));
    if (cnt.next() && cnt.value(0).toInt() == 0) {
        auto mapAllergen = [&](const QString &allergen, const QStringList &likes) {
            for (const QString &like : likes) {
                QSqlQuery ins(db);
                ins.prepare(QStringLiteral(
                    "INSERT OR IGNORE INTO allergen_food_mapping(allergen_name, food_id) "
                    "SELECT :a, id FROM foods WHERE name LIKE :like LIMIT 80"));
                ins.bindValue(QStringLiteral(":a"), allergen);
                ins.bindValue(QStringLiteral(":like"), QStringLiteral("%") + like + QStringLiteral("%"));
                ins.exec();
            }
        };
        mapAllergen(QStringLiteral("豆制品"),
                    {QStringLiteral("豆腐"), QStringLiteral("豆浆"), QStringLiteral("豆皮"),
                     QStringLiteral("腐竹"), QStringLiteral("黄豆"), QStringLiteral("大豆")});
        mapAllergen(QStringLiteral("花生"), {QStringLiteral("花生")});
        mapAllergen(QStringLiteral("牛奶"),
                    {QStringLiteral("牛奶"), QStringLiteral("奶粉"), QStringLiteral("奶油")});
        mapAllergen(QStringLiteral("鸡蛋"), {QStringLiteral("鸡蛋")});
        mapAllergen(QStringLiteral("海鲜"),
                    {QStringLiteral("虾"), QStringLiteral("蟹"), QStringLiteral("鱼"), QStringLiteral("贝")});
    }

    cnt.exec(QStringLiteral("SELECT COUNT(*) FROM nutrient_deficiency_foods"));
    if (cnt.next() && cnt.value(0).toInt() == 0) {
        auto mapDef = [&](const QString &def, const QString &nutrient) {
            QSqlQuery ins(db);
            ins.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO nutrient_deficiency_foods(deficiency_name, food_id, priority) "
                "SELECT :d, food_id, CASE concentration_level WHEN '高' THEN 1 WHEN '中' THEN 2 ELSE 4 END "
                "FROM food_nutrient_relations WHERE nutrient_id=:n LIMIT 60"));
            ins.bindValue(QStringLiteral(":d"), def);
            ins.bindValue(QStringLiteral(":n"), nutrient);
            ins.exec();
        };
        mapDef(QStringLiteral("缺铁"), QStringLiteral("铁"));
        mapDef(QStringLiteral("缺钙"), QStringLiteral("钙"));
        mapDef(QStringLiteral("缺维生素D"), QStringLiteral("维生素D"));
        mapDef(QStringLiteral("缺维生素B12"), QStringLiteral("维生素B12"));
        mapDef(QStringLiteral("蛋白质不足"), QStringLiteral("蛋白质"));
    }

    // 菜品角色修正。饼类不能仅凭“饼”字判为主食。
    {
        QSqlQuery fix(db);
        if (!fix.exec(QStringLiteral(
                "UPDATE recipes SET dish_role='snack' "
                "WHERE name IN ('土豆可乐饼','可乐土豆饼','可乐饼') "
                "   OR name LIKE '%可乐饼%'"))) {
            qWarning() << "fix snack roles:" << fix.lastError().text();
        }
        fix.exec(QStringLiteral(
            "UPDATE recipes SET dish_role='staple' WHERE name='白米饭'"));
        fix.exec(QStringLiteral(
            "UPDATE recipes SET dish_role='meat',category='午餐' "
            "WHERE name LIKE '%肉饼%' OR name LIKE '%海鲜饼%'"));
        fix.exec(QStringLiteral(
            "UPDATE recipes SET dish_role='vegetable',category='晚餐' "
            "WHERE name LIKE '%土豆饼%' OR name LIKE '%香椿饼%'"));
        // 松饼/蛋糕保留早餐适用性，但不进入午餐、晚餐的 staple 候选池。
        fix.exec(QStringLiteral(
            "UPDATE recipes SET dish_role='dessert',category='早餐' "
            "WHERE name LIKE '%松饼%' OR name LIKE '%蛋糕%'"));
    }

    // 公共菜谱去重：优先保留原料条目多、步骤完整、已核验的记录，
    // 并迁移收藏、个人库关联和推荐历史。用户自建食谱不参与公共去重。
    {
        QStringList duplicateNames;
        QSqlQuery names(db);
        if (names.exec(QStringLiteral(
                "SELECT name FROM recipes WHERE IFNULL(source_ref,'') NOT LIKE 'USER:%%' "
                "GROUP BY name HAVING COUNT(*)>1"))) {
            while (names.next())
                duplicateNames.append(names.value(0).toString());
        }

        if (!duplicateNames.isEmpty() && db.transaction()) {
            bool ok = true;
            for (const QString &name : duplicateNames) {
                QSqlQuery candidates(db);
                candidates.prepare(QStringLiteral(
                    "SELECT r.id,"
                    "(SELECT COUNT(*) FROM recipe_foods rf WHERE rf.recipe_id=r.id) AS ingredient_count,"
                    "LENGTH(IFNULL(r.steps,'')) AS step_length,"
                    "CASE WHEN IFNULL(r.nutrition_verified_at,'')<>'' THEN 1 ELSE 0 END AS verified "
                    "FROM recipes r WHERE r.name=:name "
                    "AND IFNULL(r.source_ref,'') NOT LIKE 'USER:%%' "
                    "ORDER BY ingredient_count DESC,step_length DESC,verified DESC,r.id ASC"));
                candidates.bindValue(QStringLiteral(":name"), name);
                QList<int> ids;
                if (candidates.exec()) {
                    while (candidates.next())
                        ids.append(candidates.value(0).toInt());
                }
                if (ids.size() < 2)
                    continue;
                const int keepId = ids.takeFirst();
                for (const int duplicateId : ids) {
                    auto run = [&](const QString &sql) {
                        QSqlQuery query(db);
                        query.prepare(sql);
                        query.bindValue(QStringLiteral(":keep"), keepId);
                        query.bindValue(QStringLiteral(":duplicate"), duplicateId);
                        if (!query.exec()) {
                            qWarning() << "recipe dedup:" << query.lastError().text();
                            ok = false;
                        }
                    };
                    run(QStringLiteral(
                        "INSERT OR IGNORE INTO user_favorites(user_id,item_type,item_id,created_at) "
                        "SELECT user_id,'recipe',:keep,COALESCE(NULLIF(created_at,''),datetime('now','localtime')) "
                        "FROM user_favorites WHERE item_type='recipe' AND item_id=:duplicate"));
                    run(QStringLiteral(
                        "INSERT OR IGNORE INTO favorites(user_id,recipe_id,created_at) "
                        "SELECT user_id,:keep,COALESCE(NULLIF(created_at,''),datetime('now','localtime')) "
                        "FROM favorites WHERE recipe_id=:duplicate"));
                    run(QStringLiteral(
                        "INSERT OR IGNORE INTO user_recipe_library(user_id,recipe_id,source_type,source_url,created_at) "
                        "SELECT user_id,:keep,source_type,source_url,created_at "
                        "FROM user_recipe_library WHERE recipe_id=:duplicate"));
                    run(QStringLiteral(
                        "UPDATE user_recommendation_history SET recipe_id=:keep WHERE recipe_id=:duplicate"));
                    for (const QString &sql : {
                             QStringLiteral("DELETE FROM user_favorites WHERE item_type='recipe' AND item_id=:duplicate"),
                             QStringLiteral("DELETE FROM favorites WHERE recipe_id=:duplicate"),
                             QStringLiteral("DELETE FROM user_recipe_library WHERE recipe_id=:duplicate"),
                             QStringLiteral("DELETE FROM recipe_foods WHERE recipe_id=:duplicate"),
                             QStringLiteral("DELETE FROM recipes WHERE id=:duplicate")})
                        run(sql);
                }
            }
            if (ok)
                db.commit();
            else
                db.rollback();
        }

        QSqlQuery uniqueIndex(db);
        if (!uniqueIndex.exec(QStringLiteral(
                "CREATE UNIQUE INDEX IF NOT EXISTS idx_recipes_public_unique_name ON recipes(name) "
                "WHERE IFNULL(source_ref,'') NOT LIKE 'USER:%%'"))) {
            qWarning() << "public recipe unique index:" << uniqueIndex.lastError().text();
        }
    }

    return true;
}
