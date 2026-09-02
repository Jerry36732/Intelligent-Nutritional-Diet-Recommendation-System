#include "FridgeDAO.h"
#include "DatabaseManager.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

QList<FridgeItem> FridgeDAO::listByUser(int userId) const
{
    QList<FridgeItem> list;
    if (userId <= 0 || !DatabaseManager::getInstance().isOpen())
        return list;
    QSqlQuery q(DatabaseManager::getInstance().database());
    q.prepare(QStringLiteral(
        "SELECT id, user_id, food_name, quantity, IFNULL(unit,'') AS unit, "
        "IFNULL(expiry_date,'') AS expiry_date "
        "FROM fridge_inventory WHERE user_id=:u ORDER BY updated_at DESC, id DESC"));
    q.bindValue(QStringLiteral(":u"), userId);
    if (!q.exec()) {
        qWarning() << "FridgeDAO::listByUser:" << q.lastError().text();
        return list;
    }
    while (q.next()) {
        FridgeItem item;
        item.id = q.value(0).toInt();
        item.userId = q.value(1).toInt();
        item.foodName = q.value(2).toString();
        item.quantity = q.value(3).toDouble();
        item.unit = q.value(4).toString();
        item.expiryDate = q.value(5).toString();
        list.append(item);
    }
    return list;
}

bool FridgeDAO::upsert(int userId, const QString &foodName, double quantity, const QString &unit,
                       const QString &expiryDate)
{
    const QString name = foodName.trimmed();
    const QString normalizedUnit = unit.trimmed();
    const QString normalizedExpiry = expiryDate.trimmed();
    const double amount = quantity > 0 ? quantity : 1.0;
    if (userId <= 0 || name.isEmpty() || !DatabaseManager::getInstance().isOpen())
        return false;

    QSqlDatabase db = DatabaseManager::getInstance().database();
    QSqlQuery find(db);
    find.prepare(QStringLiteral(
        "SELECT id, quantity FROM fridge_inventory "
        "WHERE user_id=:u AND food_name=:n "
        "AND IFNULL(unit,'')=:unit AND IFNULL(expiry_date,'')=:expiry "
        "ORDER BY id LIMIT 1"));
    find.bindValue(QStringLiteral(":u"), userId);
    find.bindValue(QStringLiteral(":n"), name);
    find.bindValue(QStringLiteral(":unit"), normalizedUnit);
    find.bindValue(QStringLiteral(":expiry"), normalizedExpiry);
    if (!find.exec()) {
        qWarning() << "FridgeDAO::upsert find:" << find.lastError().text();
        return false;
    }
    if (find.next()) {
        QSqlQuery upd(db);
        upd.prepare(QStringLiteral(
            "UPDATE fridge_inventory SET quantity=:q, unit=:unit, expiry_date=:expiry, "
            "updated_at=datetime('now','localtime') WHERE id=:id"));
        upd.bindValue(QStringLiteral(":q"), find.value(1).toDouble() + amount);
        upd.bindValue(QStringLiteral(":unit"), normalizedUnit);
        upd.bindValue(QStringLiteral(":expiry"), normalizedExpiry);
        upd.bindValue(QStringLiteral(":id"), find.value(0).toInt());
        return upd.exec();
    }

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO fridge_inventory(user_id, food_name, quantity, unit, expiry_date, updated_at) "
        "VALUES(:u,:n,:q,:unit,:expiry,datetime('now','localtime'))"));
    ins.bindValue(QStringLiteral(":u"), userId);
    ins.bindValue(QStringLiteral(":n"), name);
    ins.bindValue(QStringLiteral(":q"), amount);
    ins.bindValue(QStringLiteral(":unit"), normalizedUnit);
    ins.bindValue(QStringLiteral(":expiry"), normalizedExpiry);
    if (!ins.exec()) {
        qWarning() << "FridgeDAO::upsert insert:" << ins.lastError().text();
        return false;
    }
    return true;
}

bool FridgeDAO::updateById(int id, double quantity, const QString &unit,
                           const QString &expiryDate)
{
    if (id <= 0 || quantity <= 0 || !DatabaseManager::getInstance().isOpen())
        return false;
    QSqlQuery q(DatabaseManager::getInstance().database());
    q.prepare(QStringLiteral(
        "UPDATE fridge_inventory SET quantity=:q, unit=:unit, expiry_date=:expiry, "
        "updated_at=datetime('now','localtime') WHERE id=:id"));
    q.bindValue(QStringLiteral(":q"), quantity);
    q.bindValue(QStringLiteral(":unit"), unit.trimmed());
    q.bindValue(QStringLiteral(":expiry"), expiryDate.trimmed());
    q.bindValue(QStringLiteral(":id"), id);
    return q.exec();
}

bool FridgeDAO::removeById(int id)
{
    if (id <= 0 || !DatabaseManager::getInstance().isOpen())
        return false;
    QSqlQuery q(DatabaseManager::getInstance().database());
    q.prepare(QStringLiteral("DELETE FROM fridge_inventory WHERE id=:id"));
    q.bindValue(QStringLiteral(":id"), id);
    return q.exec();
}

bool FridgeDAO::clearUser(int userId)
{
    if (userId <= 0 || !DatabaseManager::getInstance().isOpen())
        return false;
    QSqlQuery q(DatabaseManager::getInstance().database());
    q.prepare(QStringLiteral("DELETE FROM fridge_inventory WHERE user_id=:u"));
    q.bindValue(QStringLiteral(":u"), userId);
    return q.exec();
}

QStringList FridgeDAO::foodNames(int userId) const
{
    QStringList names;
    for (const FridgeItem &item : listByUser(userId)) {
        if (!item.foodName.isEmpty())
            names.append(item.foodName);
    }
    return names;
}
