#ifndef USERDAO_H
#define USERDAO_H

#include "../entities/User.h"

#include <QList>
#include <QString>

class UserDAO
{
public:
    bool insertUser(const User &user);
    QList<User> findAllUsers();
    bool updateUser(const User &user);
    User findById(int id);
    User findByName(const QString &name);
    /** 校验用户名+密码，成功返回用户，失败返回无效 User */
    User authenticate(const QString &name, const QString &password);

private:
    User mapRow(const class QSqlQuery &query) const;
};

#endif // USERDAO_H
