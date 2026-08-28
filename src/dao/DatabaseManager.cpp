#include "DatabaseManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
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
            "  PRIMARY KEY (user_id, recipe_id),"
            "  FOREIGN KEY (user_id) REFERENCES users(id),"
            "  FOREIGN KEY (recipe_id) REFERENCES recipes(id)"
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
    ensureColumn(QStringLiteral("users"), QStringLiteral("preferences"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("users"), QStringLiteral("allergens"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("users"), QStringLiteral("dietary_choices"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("users"), QStringLiteral("food_intolerances"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("users"), QStringLiteral("nutritional_deficiencies"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("users"), QStringLiteral("allergies"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("users"), QStringLiteral("medical_conditions"), QStringLiteral("TEXT"));
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
    ensureColumn(QStringLiteral("recipe_foods"), QStringLiteral("display_name"), QStringLiteral("TEXT"));
    ensureColumn(QStringLiteral("recipe_foods"), QStringLiteral("source_text"), QStringLiteral("TEXT"));

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
    };
    for (const QString &sql : nactTables) {
        if (!q.exec(sql))
            qWarning() << "nact table:" << q.lastError().text();
    }

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

    return true;
}
