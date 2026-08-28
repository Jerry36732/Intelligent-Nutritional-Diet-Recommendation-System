#ifndef USERSERVICE_H
#define USERSERVICE_H

#include "../entities/User.h"

class UserService
{
public:
    UserService() = default;

    double calculateBMR(const User &user) const;
    int calculateDailyCalories(const User &user) const;
    double calculateBMI(const User &user) const;
    bool updateCalorieTarget(User &user);

    /** 保存完整用户档案（含多维健康字段），并重算热量目标 */
    bool saveUserProfile(User &user);

    void setCurrentUserId(int id);
    int currentUserId() const;
    User loadUser(int id) const;
    User loadCurrentUser() const;

private:
    int m_currentUserId = 0;
    static constexpr int kDefaultAge = 25;
};

#endif // USERSERVICE_H
