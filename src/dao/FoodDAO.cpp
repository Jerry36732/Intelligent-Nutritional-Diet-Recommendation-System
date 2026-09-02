#include "FoodDAO.h"
#include "DatabaseManager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>

namespace {
QString usdaCategoryName(const QString &category)
{
    const QString value = category.toLower();
    if (value.contains(QLatin1String("egg"))) return QStringLiteral("蛋类");
    if (value.contains(QLatin1String("dairy"))) return QStringLiteral("乳制品");
    if (value.contains(QLatin1String("pork"))) return QStringLiteral("猪肉");
    if (value.contains(QLatin1String("beef"))) return QStringLiteral("牛肉");
    if (value.contains(QLatin1String("poultry"))) return QStringLiteral("禽肉");
    if (value.contains(QLatin1String("fish")) || value.contains(QLatin1String("shellfish")))
        return QStringLiteral("水产食材");
    if (value.contains(QLatin1String("vegetable"))) return QStringLiteral("蔬菜");
    if (value.contains(QLatin1String("fruit"))) return QStringLiteral("水果");
    if (value.contains(QLatin1String("legume"))) return QStringLiteral("豆类");
    if (value.contains(QLatin1String("nut")) || value.contains(QLatin1String("seed")))
        return QStringLiteral("坚果种子");
    if (value.contains(QLatin1String("cereal")) || value.contains(QLatin1String("baked")))
        return QStringLiteral("谷物制品");
    if (value.contains(QLatin1String("fat")) || value.contains(QLatin1String("oil")))
        return QStringLiteral("油脂");
    if (value.contains(QLatin1String("spice"))) return QStringLiteral("调味料");
    if (value.contains(QLatin1String("beverage"))) return QStringLiteral("饮品");
    return QStringLiteral("食品");
}

QString translateUsdaHead(const QString &head, const QString &category)
{
    const QString value = head.toLower().trimmed();
    static const QList<QPair<QString, QString>> terms = {
        {QStringLiteral("sweet potato"), QStringLiteral("红薯")},
        {QStringLiteral("new zealand spinach"), QStringLiteral("新西兰菠菜")},
        {QStringLiteral("mustard spinach"), QStringLiteral("芥菜")},
        {QStringLiteral("sea cucumber"), QStringLiteral("海参")},
        {QStringLiteral("sea bass"), QStringLiteral("海鲈鱼")},
        {QStringLiteral("soy milk"), QStringLiteral("豆奶")},
        {QStringLiteral("almond milk"), QStringLiteral("杏仁奶")},
        {QStringLiteral("apple juice"), QStringLiteral("苹果汁")},
        {QStringLiteral("orange juice"), QStringLiteral("橙汁")},
        {QStringLiteral("grape juice"), QStringLiteral("葡萄汁")},
        {QStringLiteral("grapefruit juice"), QStringLiteral("西柚汁")},
        {QStringLiteral("pineapple juice"), QStringLiteral("菠萝汁")},
        {QStringLiteral("tomato juice"), QStringLiteral("番茄汁")},
        {QStringLiteral("salad dressing"), QStringLiteral("沙拉酱")},
        {QStringLiteral("sesame butter"), QStringLiteral("芝麻酱")},
        {QStringLiteral("blackeye pea"), QStringLiteral("黑眼豆")},
        {QStringLiteral("cowpeas"), QStringLiteral("豇豆")},
        {QStringLiteral("lima beans"), QStringLiteral("利马豆")},
        {QStringLiteral("lotus root"), QStringLiteral("莲藕")},
        {QStringLiteral("brussels sprouts"), QStringLiteral("抱子甘蓝")},
        {QStringLiteral("steelhead trout"), QStringLiteral("虹鳟鱼")},
        {QStringLiteral("snow crab"), QStringLiteral("雪蟹")},
        {QStringLiteral("squid"), QStringLiteral("鱿鱼")},
        {QStringLiteral("vital wheat gluten"), QStringLiteral("谷朊粉")},
        {QStringLiteral("sorghum flour"), QStringLiteral("高粱粉")},
        {QStringLiteral("sorghum bran"), QStringLiteral("高粱麸皮")},
        {QStringLiteral("wild rice"), QStringLiteral("菰米")},
        {QStringLiteral("pork loin"), QStringLiteral("猪里脊")},
        {QStringLiteral("pork"), QStringLiteral("猪肉")},
        {QStringLiteral("beef"), QStringLiteral("牛肉")},
        {QStringLiteral("chicken"), QStringLiteral("鸡肉")},
        {QStringLiteral("turkey"), QStringLiteral("火鸡肉")},
        {QStringLiteral("ham"), QStringLiteral("火腿")},
        {QStringLiteral("sausage"), QStringLiteral("香肠")},
        {QStringLiteral("frankfurter"), QStringLiteral("法兰克福香肠")},
        {QStringLiteral("bologna"), QStringLiteral("博洛尼亚香肠")},
        {QStringLiteral("fish"), QStringLiteral("鱼肉")},
        {QStringLiteral("tuna"), QStringLiteral("金枪鱼")},
        {QStringLiteral("snapper"), QStringLiteral("笛鲷")},
        {QStringLiteral("swordfish"), QStringLiteral("剑鱼")},
        {QStringLiteral("scallops"), QStringLiteral("扇贝")},
        {QStringLiteral("crustaceans"), QStringLiteral("甲壳类水产")},
        {QStringLiteral("egg"), QStringLiteral("鸡蛋")},
        {QStringLiteral("cheese"), QStringLiteral("奶酪")},
        {QStringLiteral("yogurt"), QStringLiteral("酸奶")},
        {QStringLiteral("milk"), QStringLiteral("牛奶")},
        {QStringLiteral("cream"), QStringLiteral("奶油")},
        {QStringLiteral("tofu"), QStringLiteral("豆腐")},
        {QStringLiteral("soybeans"), QStringLiteral("黄豆")},
        {QStringLiteral("lentils"), QStringLiteral("扁豆")},
        {QStringLiteral("beans"), QStringLiteral("豆类")},
        {QStringLiteral("hummus"), QStringLiteral("鹰嘴豆泥")},
        {QStringLiteral("mushroom"), QStringLiteral("蘑菇")},
        {QStringLiteral("tomato"), QStringLiteral("番茄")},
        {QStringLiteral("potato"), QStringLiteral("土豆")},
        {QStringLiteral("pepper"), QStringLiteral("辣椒")},
        {QStringLiteral("cabbage"), QStringLiteral("卷心菜")},
        {QStringLiteral("lettuce"), QStringLiteral("生菜")},
        {QStringLiteral("carrot"), QStringLiteral("胡萝卜")},
        {QStringLiteral("broccoli"), QStringLiteral("西兰花")},
        {QStringLiteral("cauliflower"), QStringLiteral("菜花")},
        {QStringLiteral("spinach"), QStringLiteral("菠菜")},
        {QStringLiteral("kale"), QStringLiteral("羽衣甘蓝")},
        {QStringLiteral("asparagus"), QStringLiteral("芦笋")},
        {QStringLiteral("artichoke"), QStringLiteral("洋蓟")},
        {QStringLiteral("beet greens"), QStringLiteral("甜菜叶")},
        {QStringLiteral("beet"), QStringLiteral("甜菜根")},
        {QStringLiteral("collards"), QStringLiteral("羽衣甘蓝叶")},
        {QStringLiteral("cucumber"), QStringLiteral("黄瓜")},
        {QStringLiteral("onion"), QStringLiteral("洋葱")},
        {QStringLiteral("leek"), QStringLiteral("韭葱")},
        {QStringLiteral("radish"), QStringLiteral("萝卜")},
        {QStringLiteral("turnip"), QStringLiteral("芜菁")},
        {QStringLiteral("squash"), QStringLiteral("南瓜")},
        {QStringLiteral("gourd"), QStringLiteral("葫芦瓜")},
        {QStringLiteral("bitter gourd"), QStringLiteral("苦瓜")},
        {QStringLiteral("corn"), QStringLiteral("玉米")},
        {QStringLiteral("succotash"), QStringLiteral("玉米杂豆")},
        {QStringLiteral("apple"), QStringLiteral("苹果")},
        {QStringLiteral("banana"), QStringLiteral("香蕉")},
        {QStringLiteral("grape"), QStringLiteral("葡萄")},
        {QStringLiteral("grapefruit"), QStringLiteral("西柚")},
        {QStringLiteral("peach"), QStringLiteral("桃")},
        {QStringLiteral("pineapple"), QStringLiteral("菠萝")},
        {QStringLiteral("strawberr"), QStringLiteral("草莓")},
        {QStringLiteral("watermelon"), QStringLiteral("西瓜")},
        {QStringLiteral("kiwifruit"), QStringLiteral("猕猴桃")},
        {QStringLiteral("nectarine"), QStringLiteral("油桃")},
        {QStringLiteral("mango"), QStringLiteral("芒果")},
        {QStringLiteral("melon"), QStringLiteral("甜瓜")},
        {QStringLiteral("plantain"), QStringLiteral("大蕉")},
        {QStringLiteral("nuts"), QStringLiteral("坚果")},
        {QStringLiteral("almond"), QStringLiteral("杏仁")},
        {QStringLiteral("seeds"), QStringLiteral("种子")},
        {QStringLiteral("flour"), QStringLiteral("面粉")},
        {QStringLiteral("cornmeal"), QStringLiteral("玉米面")},
        {QStringLiteral("rice"), QStringLiteral("米饭")},
        {QStringLiteral("oats"), QStringLiteral("燕麦")},
        {QStringLiteral("bread"), QStringLiteral("面包")},
        {QStringLiteral("bagel"), QStringLiteral("贝果")},
        {QStringLiteral("waffle"), QStringLiteral("华夫饼")},
        {QStringLiteral("cracker"), QStringLiteral("薄脆饼干")},
        {QStringLiteral("cookies"), QStringLiteral("曲奇饼干")},
        {QStringLiteral("cereal"), QStringLiteral("早餐谷物")},
        {QStringLiteral("oil"), QStringLiteral("食用油")},
        {QStringLiteral("mustard"), QStringLiteral("芥末酱")},
        {QStringLiteral("pickles"), QStringLiteral("腌黄瓜")},
        {QStringLiteral("sauce"), QStringLiteral("酱汁")},
        {QStringLiteral("candies"), QStringLiteral("糖果")},
        {QStringLiteral("pudding"), QStringLiteral("布丁")},
        {QStringLiteral("snacks"), QStringLiteral("零食")},
        {QStringLiteral("babyfood"), QStringLiteral("婴幼儿食品")},
        {QStringLiteral("beverage"), QStringLiteral("饮品")},
        {QStringLiteral("juice"), QStringLiteral("果汁")},
        {QStringLiteral("restaurant"), QStringLiteral("餐厅食品")},
    };
    for (const auto &term : terms) {
        if (value.contains(term.first))
            return term.second;
    }
    return usdaCategoryName(category) + QStringLiteral("食材");
}

QString translateUsdaDetail(const QString &detail)
{
    const QString value = detail.toLower().trimmed();
    static const QList<QPair<QString, QString>> terms = {
        {QStringLiteral("without salt"), QStringLiteral("无盐")},
        {QStringLiteral("with salt added"), QStringLiteral("加盐")},
        {QStringLiteral("low sodium"), QStringLiteral("低钠")},
        {QStringLiteral("fat free"), QStringLiteral("脱脂")},
        {QStringLiteral("nonfat"), QStringLiteral("脱脂")},
        {QStringLiteral("lowfat"), QStringLiteral("低脂")},
        {QStringLiteral("part-skim"), QStringLiteral("部分脱脂")},
        {QStringLiteral("skinless"), QStringLiteral("去皮")},
        {QStringLiteral("boneless"), QStringLiteral("去骨")},
        {QStringLiteral("meat only"), QStringLiteral("仅瘦肉")},
        {QStringLiteral("drained solids"), QStringLiteral("沥干固形物")},
        {QStringLiteral("ready-to-serve"), QStringLiteral("即食")},
        {QStringLiteral("dry roasted"), QStringLiteral("干烤")},
        {QStringLiteral("oven roasted"), QStringLiteral("烤制")},
        {QStringLiteral("cooked"), QStringLiteral("熟制")},
        {QStringLiteral("boiled"), QStringLiteral("水煮")},
        {QStringLiteral("braised"), QStringLiteral("炖制")},
        {QStringLiteral("fried"), QStringLiteral("油炸")},
        {QStringLiteral("baked"), QStringLiteral("烘焙")},
        {QStringLiteral("grilled"), QStringLiteral("烤制")},
        {QStringLiteral("canned"), QStringLiteral("罐装")},
        {QStringLiteral("frozen"), QStringLiteral("冷冻")},
        {QStringLiteral("dried"), QStringLiteral("干制")},
        {QStringLiteral("raw"), QStringLiteral("生鲜")},
        {QStringLiteral("pasteurized"), QStringLiteral("巴氏杀菌")},
        {QStringLiteral("whole"), QStringLiteral("完整")},
        {QStringLiteral("white"), QStringLiteral("蛋清")},
        {QStringLiteral("yolk"), QStringLiteral("蛋黄")},
        {QStringLiteral("breast"), QStringLiteral("胸肉")},
        {QStringLiteral("drumstick"), QStringLiteral("小腿肉")},
        {QStringLiteral("loin"), QStringLiteral("里脊")},
        {QStringLiteral("ground"), QStringLiteral("绞碎")},
        {QStringLiteral("sliced"), QStringLiteral("切片")},
        {QStringLiteral("diced"), QStringLiteral("切丁")},
        {QStringLiteral("grated"), QStringLiteral("磨碎")},
        {QStringLiteral("plain"), QStringLiteral("原味")},
        {QStringLiteral("sweetened"), QStringLiteral("加糖")},
        {QStringLiteral("unsweetened"), QStringLiteral("无糖")},
        {QStringLiteral("parmesan"), QStringLiteral("帕尔马干酪")},
        {QStringLiteral("cheddar"), QStringLiteral("切达奶酪")},
        {QStringLiteral("mozzarella"), QStringLiteral("马苏里拉奶酪")},
        {QStringLiteral("cottage"), QStringLiteral("茅屋奶酪")},
        {QStringLiteral("greek"), QStringLiteral("希腊式")},
        {QStringLiteral("strawberry"), QStringLiteral("草莓味")},
        {QStringLiteral("coconut"), QStringLiteral("椰子")},
        {QStringLiteral("sunflower"), QStringLiteral("葵花籽")},
        {QStringLiteral("green"), QStringLiteral("绿色")},
        {QStringLiteral("yellow"), QStringLiteral("黄色")},
        {QStringLiteral("red"), QStringLiteral("红色")},
    };
    for (const auto &term : terms) {
        if (value.contains(term.first))
            return term.second;
    }
    return {};
}

QString localizedFoodName(const QString &name, const QString &category)
{
    if (name == QStringLiteral("微量调料：b料：胡椒粉"))
        return QStringLiteral("胡椒粉");
    if (!name.startsWith(QStringLiteral("[USDA]"), Qt::CaseInsensitive))
        return name;
    const QString description = name.mid(6).trimmed();
    const QStringList parts = description.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return usdaCategoryName(category) + QStringLiteral("食材");
    QStringList translated{translateUsdaHead(parts.first(), category)};
    for (int i = 1; i < parts.size() && translated.size() < 4; ++i) {
        QString detail = translateUsdaDetail(parts.at(i));
        if (translated.first() == QStringLiteral("鸡蛋")
            && parts.at(i).contains(QStringLiteral("whole"), Qt::CaseInsensitive)) {
            detail = QStringLiteral("全蛋");
        }
        if (!detail.isEmpty() && !translated.contains(detail))
            translated.append(detail);
    }
    return translated.join(QStringLiteral("·"));
}
} // namespace

Food FoodDAO::mapRow(const QSqlQuery &query) const
{
    Food f;
    f.id = query.value(QStringLiteral("id")).toInt();
    const QString rawName = query.value(QStringLiteral("name")).toString();

    const QString label = query.value(QStringLiteral("category_label")).toString();
    if (!label.isEmpty())
        f.category = label;
    else {
        const QVariant cat = query.value(QStringLiteral("category_one"));
        f.category = cat.isNull() ? QString() : QString::number(cat.toInt());
    }
    f.name = localizedFoodName(rawName, f.category);

    f.calories = query.value(QStringLiteral("calories")).toDouble();
    f.protein = query.value(QStringLiteral("protein")).toDouble();
    f.carbs = query.value(QStringLiteral("carbs")).toDouble();
    f.fat = query.value(QStringLiteral("fat")).toDouble();
    f.unit = query.value(QStringLiteral("unit")).toString();
    return f;
}

QList<Food> FoodDAO::findAll(int limit)
{
    QList<Food> list;
    QSqlDatabase db = DatabaseManager::getInstance().database();
    if (!db.isOpen()) {
        qWarning() << "FoodDAO::findAll: database not open";
        return list;
    }

    // 避免 SQLite 对绑定 LIMIT 支持不一致导致整表查空
    const int safeLimit = limit > 0 ? limit : 3000;
    QSqlQuery q(db);
    const QString sql = QStringLiteral(
        "SELECT id, name, category_one, category_label, calories, protein, carbs, fat, unit "
        "FROM foods ORDER BY id LIMIT %1").arg(safeLimit);
    if (!q.exec(sql)) {
        qWarning() << "FoodDAO::findAll:" << q.lastError().text();
        return list;
    }
    while (q.next())
        list.append(mapRow(q));
    if (list.isEmpty())
        qWarning() << "FoodDAO::findAll: query ok but 0 rows";
    return list;
}

QList<Food> FoodDAO::searchByName(const QString &keyword)
{
    QList<Food> list;
    QSqlDatabase db = DatabaseManager::getInstance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT id, name, category_one, category_label, calories, protein, carbs, fat, unit "
        "FROM foods WHERE name LIKE :kw OR IFNULL(category_label,'') LIKE :kw "
        "ORDER BY name LIMIT 200"));
    q.bindValue(QStringLiteral(":kw"), QStringLiteral("%") + keyword + QStringLiteral("%"));
    if (!q.exec()) {
        qWarning() << "FoodDAO::searchByName:" << q.lastError().text();
        return list;
    }
    while (q.next())
        list.append(mapRow(q));
    return list;
}

QList<Food> FoodDAO::findFavorites(int userId)
{
    QList<Food> list;
    if (userId <= 0)
        return list;
    QSqlQuery q(DatabaseManager::getInstance().database());
    q.prepare(QStringLiteral(
        "SELECT f.id, f.name, f.category_one, f.category_label, f.calories, f.protein, "
        "f.carbs, f.fat, f.unit FROM foods f "
        "JOIN user_favorites ff ON ff.item_id=f.id "
        "WHERE ff.user_id=:uid AND ff.item_type='ingredient' ORDER BY f.name"));
    q.bindValue(QStringLiteral(":uid"), userId);
    if (!q.exec()) {
        qWarning() << "FoodDAO::findFavorites:" << q.lastError().text();
        return list;
    }
    while (q.next())
        list.append(mapRow(q));
    return list;
}

bool FoodDAO::isFavorite(int userId, int foodId)
{
    QSqlQuery q(DatabaseManager::getInstance().database());
    q.prepare(QStringLiteral(
        "SELECT 1 FROM user_favorites WHERE user_id=:uid "
        "AND item_type='ingredient' AND item_id=:fid LIMIT 1"));
    q.bindValue(QStringLiteral(":uid"), userId);
    q.bindValue(QStringLiteral(":fid"), foodId);
    return q.exec() && q.next();
}

bool FoodDAO::toggleFavorite(int userId, int foodId)
{
    return setFavorite(userId, foodId, !isFavorite(userId, foodId));
}

bool FoodDAO::setFavorite(int userId, int foodId, bool favorite)
{
    if (userId <= 0 || foodId <= 0)
        return false;
    QSqlDatabase db = DatabaseManager::getInstance().database();
    if (!favorite) {
        if (!db.transaction())
            return false;
        QSqlQuery remove(db);
        remove.prepare(QStringLiteral(
            "DELETE FROM user_favorites WHERE user_id=:uid "
            "AND item_type='ingredient' AND item_id=:fid"));
        remove.bindValue(QStringLiteral(":uid"), userId);
        remove.bindValue(QStringLiteral(":fid"), foodId);
        if (!remove.exec()) {
            db.rollback();
            return false;
        }
        QSqlQuery legacy(db);
        legacy.prepare(QStringLiteral(
            "DELETE FROM food_favorites WHERE user_id=:uid AND food_id=:fid"));
        legacy.bindValue(QStringLiteral(":uid"), userId);
        legacy.bindValue(QStringLiteral(":fid"), foodId);
        return legacy.exec() && db.commit();
    }
    if (!db.transaction())
        return false;
    QSqlQuery add(db);
    add.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO user_favorites(user_id,item_type,item_id,created_at) "
        "VALUES(:uid,'ingredient',:fid,datetime('now','localtime'))"));
    add.bindValue(QStringLiteral(":uid"), userId);
    add.bindValue(QStringLiteral(":fid"), foodId);
    if (!add.exec()) {
        db.rollback();
        return false;
    }
    QSqlQuery legacy(db);
    legacy.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO food_favorites(user_id,food_id,created_at) "
        "VALUES(:uid,:fid,datetime('now','localtime'))"));
    legacy.bindValue(QStringLiteral(":uid"), userId);
    legacy.bindValue(QStringLiteral(":fid"), foodId);
    return legacy.exec() && db.commit();
}

int FoodDAO::count()
{
    QSqlDatabase db = DatabaseManager::getInstance().database();
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM foods"))) {
        qWarning() << "FoodDAO::count:" << q.lastError().text();
        return 0;
    }
    if (q.next())
        return q.value(0).toInt();
    return 0;
}
