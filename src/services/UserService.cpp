#include "UserService.h"
#include "../dao/UserDAO.h"

#include <QtMath>

double UserService::calculateBMR(const User &user) const
{
    // Harris-Benedict (revised), keeping 25 as a safe fallback for legacy profiles.
    const double w = user.weight;
    const double h = user.height;
    const double age = static_cast<double>(user.age > 0 ? user.age : kDefaultAge);

    if (user.gender.compare(QStringLiteral("female"), Qt::CaseInsensitive) == 0)
        return 655.1 + (9.563 * w) + (1.850 * h) - (4.676 * age);

    // male (default)
    return 66.47 + (13.75 * w) + (5.003 * h) - (6.755 * age);
}

int UserService::calculateDailyCalories(const User &user) const
{
    const double bmr = calculateBMR(user);
    double calories = bmr * 1.55; // moderate activity

    const QString goal = user.goal.toLower();
    if (goal == QLatin1String("lose"))
        calories *= 0.85;
    else if (goal == QLatin1String("gain"))
        calories *= 1.15;
    // maintain: * 1.0

    // 医疗状况微调（保守）
    if (user.medicalConditions.contains(QStringLiteral("肥胖")))
        calories *= 0.95;
    if (user.medicalConditions.contains(QStringLiteral("2型糖尿病"))
        || user.medicalConditions.contains(QStringLiteral("糖尿病")))
        calories *= 0.97;

    return static_cast<int>(qRound(calories));
}

double UserService::calculateBMI(const User &user) const
{
    if (user.height <= 0.0)
        return 0.0;
    const double meters = user.height / 100.0;
    return user.weight / (meters * meters);
}

bool UserService::updateCalorieTarget(User &user)
{
    user.calorieTarget = calculateDailyCalories(user);
    UserDAO dao;
    return dao.updateUser(user);
}

bool UserService::saveUserProfile(User &user)
{
    user.syncAllergenFields();
    user.calorieTarget = calculateDailyCalories(user);
    UserDAO dao;
    if (!dao.updateUser(user))
        return false;
    User fresh = dao.findById(user.id);
    if (fresh.id > 0)
        user = fresh;
    return true;
}

void UserService::setCurrentUserId(int id)
{
    m_currentUserId = id;
}

int UserService::currentUserId() const
{
    return m_currentUserId;
}

User UserService::loadUser(int id) const
{
    UserDAO dao;
    return dao.findById(id);
}

User UserService::loadCurrentUser() const
{
    if (m_currentUserId <= 0)
        return User{};
    return loadUser(m_currentUserId);
}
