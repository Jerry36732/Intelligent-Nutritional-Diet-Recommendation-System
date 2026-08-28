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
    QList<RecipeIngredient> getIngredients(int recipeId);
    QList<Recipe> findFavorites(int userId);
    bool toggleFavorite(int userId, int recipeId);
    bool isFavorite(int userId, int recipeId);

private:
    Recipe mapRow(const class QSqlQuery &query) const;
};

#endif // RECIPEDAO_H
