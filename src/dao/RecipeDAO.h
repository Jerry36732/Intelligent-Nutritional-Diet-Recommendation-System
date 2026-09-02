#ifndef RECIPEDAO_H
#define RECIPEDAO_H

#include "../entities/Recipe.h"

#include <QList>
#include <QString>

class RecipeDAO
{
public:
    QList<Recipe> findByCategory(const QString &category);
    QList<Recipe> findByRole(const QString &role);
    Recipe findById(int id);
    QList<Recipe> findAll();
    QList<Recipe> browse(const QString &search, const QString &filterKey,
                         int userId, int offset, int limit, int *total = nullptr);
    QList<RecipeIngredient> getIngredients(int recipeId);
    QList<Recipe> findFavorites(int userId);
    bool toggleFavorite(int userId, int recipeId);
    bool setFavorite(int userId, int recipeId, bool favorite);
    bool isFavorite(int userId, int recipeId);

private:
    Recipe mapRow(const class QSqlQuery &query) const;
};

#endif // RECIPEDAO_H
