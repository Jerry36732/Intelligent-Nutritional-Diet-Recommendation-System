#ifndef FOODDAO_H
#define FOODDAO_H

#include "../entities/Food.h"

#include <QList>
#include <QString>

class FoodDAO
{
public:
    QList<Food> findAll(int limit = 100);
    QList<Food> searchByName(const QString &keyword);
    QList<Food> findFavorites(int userId);
    bool isFavorite(int userId, int foodId);
    bool toggleFavorite(int userId, int foodId);
    bool setFavorite(int userId, int foodId, bool favorite);
    int count();

private:
    Food mapRow(const class QSqlQuery &query) const;
};

#endif // FOODDAO_H
