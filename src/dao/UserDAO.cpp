#include "UserDAO.h"
#include "DatabaseManager.h"
#include "../services/AuthUtils.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

namespace {
QString colOrEmpty(const QSqlQuery &query, const QString &name)
{
    const int idx = query.record().indexOf(name);
    if (idx < 0)
        return {};
    return query.value(idx).toString();
}
} // namespace

User UserDAO::mapRow(const QSqlQuery &query) const
{
    User u;
    u.id = query.value(QStringLiteral("id")).toInt();
    u.name = query.value(QStringLiteral("name")).toString();
    u.gender = query.value(QStringLiteral("gender")).toString();
    u.goal = query.value(QStringLiteral("goal")).toString();
    u.height = query.value(QStringLiteral("height")).toDouble();
    u.weight = query.value(QStringLiteral("weight")).toDouble();
    u.calorieTarget = query.value(QStringLiteral("calorie_target")).toInt();
    u.passwordHash = query.value(QStringLiteral("password_hash")).toString();
    u.preferences = query.value(QStringLiteral("preferences")).toString();
    u.allergens = query.value(QStringLiteral("allergens")).toString();

    u.dietaryChoices = User::stringListFromJson(colOrEmpty(query, QStringLiteral("dietary_choices")));
    u.foodIntolerances = User::stringListFromJson(colOrEmpty(query, QStringLiteral("food_intolerances")));
    u.nutritionalDeficiencies =
        User::stringListFromJson(colOrEmpty(query, QStringLiteral("nutritional_deficiencies")));
    u.allergies = User::stringListFromJson(colOrEmpty(query, QStringLiteral("allergies")));
    u.medicalConditions = User::stringListFromJson(colOrEmpty(query, QStringLiteral("medical_conditions")));
    u.syncAllergenFields();
    return u;
}

static const char *kUserSelectCols =
    "id, name, gender, goal, height, weight, calorie_target, password_hash, "
    "IFNULL(preferences,'') AS preferences, IFNULL(allergens,'') AS allergens, "
    "IFNULL(dietary_choices,'[]') AS dietary_choices, "
    "IFNULL(food_intolerances,'[]') AS food_intolerances, "
    "IFNULL(nutritional_deficiencies,'[]') AS nutritional_deficiencies, "
    "IFNULL(allergies,'[]') AS allergies, "
    "IFNULL(medical_conditions,'[]') AS medical_conditions";

bool UserDAO::insertUser(const User &user)
{
    if (!DatabaseManager::getInstance().isOpen()) {
        qWarning() << "UserDAO::insertUser: database not open";
        return false;
    }
    QSqlDatabase db = DatabaseManager::getInstance().database();
    if (!db.isOpen()) {
        qWarning() << "UserDAO::insertUser: connection closed";
        return false;
    }

    User u = user;
    u.syncAllergenFields();

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO users (name, gender, goal, height, weight, calorie_target, password_hash, "
        "preferences, allergens, dietary_choices, food_intolerances, nutritional_deficiencies, "
        "allergies, medical_conditions) "
        "VALUES (:name, :gender, :goal, :height, :weight, :calorie_target, :password_hash, "
        ":preferences, :allergens, :dietary_choices, :food_intolerances, :nutritional_deficiencies, "
        ":allergies, :medical_conditions)"));
    q.bindValue(QStringLiteral(":name"), u.name);
    q.bindValue(QStringLiteral(":gender"), u.gender);
    q.bindValue(QStringLiteral(":goal"), u.goal);
    q.bindValue(QStringLiteral(":height"), u.height);
    q.bindValue(QStringLiteral(":weight"), u.weight);
    q.bindValue(QStringLiteral(":calorie_target"), u.calorieTarget);
    q.bindValue(QStringLiteral(":password_hash"), u.passwordHash);
    q.bindValue(QStringLiteral(":preferences"), u.preferences);
    q.bindValue(QStringLiteral(":allergens"), u.allergens);
    q.bindValue(QStringLiteral(":dietary_choices"), User::stringListToJson(u.dietaryChoices));
    q.bindValue(QStringLiteral(":food_intolerances"), User::stringListToJson(u.foodIntolerances));
    q.bindValue(QStringLiteral(":nutritional_deficiencies"),
                User::stringListToJson(u.nutritionalDeficiencies));
    q.bindValue(QStringLiteral(":allergies"), User::stringListToJson(u.allergies));
    q.bindValue(QStringLiteral(":medical_conditions"), User::stringListToJson(u.medicalConditions));

    if (!q.exec()) {
        qWarning() << "UserDAO::insertUser:" << q.lastError().text();
        return false;
    }
    return true;
}

QList<User> UserDAO::findAllUsers()
{
    QList<User> list;
    QSqlDatabase db = DatabaseManager::getInstance().database();
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT %1 FROM users ORDER BY id").arg(QLatin1String(kUserSelectCols)))) {
        qWarning() << "UserDAO::findAllUsers:" << q.lastError().text();
        return list;
    }
    while (q.next())
        list.append(mapRow(q));
    return list;
}

bool UserDAO::updateUser(const User &user)
{
    User u = user;
    u.syncAllergenFields();

    QSqlDatabase db = DatabaseManager::getInstance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE users SET name = :name, gender = :gender, goal = :goal, "
        "height = :height, weight = :weight, calorie_target = :calorie_target, "
        "password_hash = COALESCE(NULLIF(:password_hash, ''), password_hash), "
        "preferences = :preferences, allergens = :allergens, "
        "dietary_choices = :dietary_choices, food_intolerances = :food_intolerances, "
        "nutritional_deficiencies = :nutritional_deficiencies, allergies = :allergies, "
        "medical_conditions = :medical_conditions "
        "WHERE id = :id"));
    q.bindValue(QStringLiteral(":name"), u.name);
    q.bindValue(QStringLiteral(":gender"), u.gender);
    q.bindValue(QStringLiteral(":goal"), u.goal);
    q.bindValue(QStringLiteral(":height"), u.height);
    q.bindValue(QStringLiteral(":weight"), u.weight);
    q.bindValue(QStringLiteral(":calorie_target"), u.calorieTarget);
    q.bindValue(QStringLiteral(":password_hash"), u.passwordHash);
    q.bindValue(QStringLiteral(":preferences"), u.preferences);
    q.bindValue(QStringLiteral(":allergens"), u.allergens);
    q.bindValue(QStringLiteral(":dietary_choices"), User::stringListToJson(u.dietaryChoices));
    q.bindValue(QStringLiteral(":food_intolerances"), User::stringListToJson(u.foodIntolerances));
    q.bindValue(QStringLiteral(":nutritional_deficiencies"),
                User::stringListToJson(u.nutritionalDeficiencies));
    q.bindValue(QStringLiteral(":allergies"), User::stringListToJson(u.allergies));
    q.bindValue(QStringLiteral(":medical_conditions"), User::stringListToJson(u.medicalConditions));
    q.bindValue(QStringLiteral(":id"), u.id);

    if (!q.exec()) {
        qWarning() << "UserDAO::updateUser:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() >= 0;
}

User UserDAO::findById(int id)
{
    QSqlDatabase db = DatabaseManager::getInstance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT %1 FROM users WHERE id = :id").arg(QLatin1String(kUserSelectCols)));
    q.bindValue(QStringLiteral(":id"), id);
    if (!q.exec()) {
        qWarning() << "UserDAO::findById:" << q.lastError().text();
        return User{};
    }
    if (q.next())
        return mapRow(q);
    return User{};
}

User UserDAO::findByName(const QString &name)
{
    if (!DatabaseManager::getInstance().isOpen())
        return User{};
    QSqlDatabase db = DatabaseManager::getInstance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT %1 FROM users WHERE name = :name").arg(QLatin1String(kUserSelectCols)));
    q.bindValue(QStringLiteral(":name"), name);
    if (!q.exec()) {
        qWarning() << "UserDAO::findByName:" << q.lastError().text();
        return User{};
    }
    if (q.next())
        return mapRow(q);
    return User{};
}

User UserDAO::authenticate(const QString &name, const QString &password)
{
    User u = findByName(name);
    if (u.id <= 0)
        return User{};
    if (!AuthUtils::verifyPassword(password, u.passwordHash))
        return User{};
    u.passwordHash.clear();
    return u;
}
