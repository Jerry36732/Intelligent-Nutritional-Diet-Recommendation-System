#include "RecommendationHistoryDAO.h"
#include "DatabaseManager.h"

#include <QDate>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

bool RecommendationHistoryDAO::recordPlan(int userId, const RecommendResult &plan)
{
    if (userId <= 0 || !plan.valid || !DatabaseManager::getInstance().isOpen())
        return false;

    QSqlDatabase db = DatabaseManager::getInstance().database();
    const QString today = QDate::currentDate().toString(Qt::ISODate);

    auto insertMeal = [&](const MealSlot &slot) {
        for (const Recipe &r : slot.dishes) {
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO user_recommendation_history"
                "(user_id, recipe_id, recipe_name, meal_label, recommended_on) "
                "VALUES(:u,:r,:n,:m,:d)"));
            q.bindValue(QStringLiteral(":u"), userId);
            q.bindValue(QStringLiteral(":r"), r.id);
            q.bindValue(QStringLiteral(":n"), r.name);
            q.bindValue(QStringLiteral(":m"), slot.mealLabel);
            q.bindValue(QStringLiteral(":d"), today);
            if (!q.exec())
                qWarning() << "recordPlan:" << q.lastError().text();
        }
    };
    insertMeal(plan.breakfast);
    insertMeal(plan.lunch);
    insertMeal(plan.dinner);
    purgeOlderThan(userId, 7);
    return true;
}

QSet<int> RecommendationHistoryDAO::recentRecipeIds(int userId, int days) const
{
    QSet<int> ids;
    if (userId <= 0 || !DatabaseManager::getInstance().isOpen())
        return ids;
    QSqlQuery q(DatabaseManager::getInstance().database());
    q.prepare(QStringLiteral(
        "SELECT DISTINCT recipe_id FROM user_recommendation_history "
        "WHERE user_id=:u AND recommended_on >= date('now','localtime', :offset)"));
    q.bindValue(QStringLiteral(":u"), userId);
    q.bindValue(QStringLiteral(":offset"), QStringLiteral("-%1 days").arg(days));
    if (!q.exec()) {
        qWarning() << "recentRecipeIds:" << q.lastError().text();
        return ids;
    }
    while (q.next())
        ids.insert(q.value(0).toInt());
    return ids;
}

void RecommendationHistoryDAO::purgeOlderThan(int userId, int keepDays) const
{
    if (!DatabaseManager::getInstance().isOpen())
        return;
    QSqlQuery q(DatabaseManager::getInstance().database());
    q.prepare(QStringLiteral(
        "DELETE FROM user_recommendation_history "
        "WHERE user_id=:u AND recommended_on < date('now','localtime', :offset)"));
    q.bindValue(QStringLiteral(":u"), userId);
    q.bindValue(QStringLiteral(":offset"), QStringLiteral("-%1 days").arg(keepDays));
    q.exec();
}
