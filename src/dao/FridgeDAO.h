#ifndef FRIDGEDAO_H
#define FRIDGEDAO_H

#include "../entities/FridgeItem.h"

#include <QList>
#include <QString>

class FridgeDAO
{
public:
    QList<FridgeItem> listByUser(int userId) const;
    bool upsert(int userId, const QString &foodName, double quantity = 1.0,
                const QString &unit = QString(), const QString &expiryDate = QString());
    bool updateById(int id, double quantity, const QString &unit,
                    const QString &expiryDate);
    bool removeById(int id);
    bool clearUser(int userId);
    QStringList foodNames(int userId) const;
};

#endif // FRIDGEDAO_H
