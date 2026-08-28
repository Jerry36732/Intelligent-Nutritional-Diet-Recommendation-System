#include "FoodDAO.h"
#include "DatabaseManager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>

Food FoodDAO::mapRow(const QSqlQuery &query) const
{
    Food f;
    f.id = query.value(QStringLiteral("id")).toInt();
    f.name = query.value(QStringLiteral("name")).toString();

    const QString label = query.value(QStringLiteral("category_label")).toString();
    if (!label.isEmpty())
        f.category = label;
    else {
        const QVariant cat = query.value(QStringLiteral("category_one"));
        f.category = cat.isNull() ? QString() : QString::number(cat.toInt());
    }

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
