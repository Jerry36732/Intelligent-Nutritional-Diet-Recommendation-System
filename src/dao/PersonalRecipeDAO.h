#ifndef PERSONALRECIPEDAO_H
#define PERSONALRECIPEDAO_H

#include "../entities/Recipe.h"

#include <QList>
#include <QString>

struct PersonalRecipeIngredient
{
    QString name;
    double quantity = 0.0;
    QString quantityText;
};

struct PersonalRecipeDraft
{
    QString name;
    QString category;
    QString dishRole;
    QString steps;
    int cookMinutes = 20;
    QString sourceType;
    QString sourceUrl;
    QList<PersonalRecipeIngredient> ingredients;
};

class PersonalRecipeDAO
{
public:
    int create(int userId, const PersonalRecipeDraft &draft, QString *errorMessage = nullptr) const;
    QList<Recipe> findByUser(int userId) const;
    bool isPersonal(int userId, int recipeId) const;
};

#endif // PERSONALRECIPEDAO_H
