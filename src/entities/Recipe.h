#ifndef RECIPE_H
#define RECIPE_H

#include <QList>
#include <QMetaType>
#include <QString>

struct RecipeIngredient
{
    QString foodName;
    double quantity = 0.0;
};

struct Recipe
{
    int id = 0;
    QString name;
    QString category;        // 早餐 / 午餐 / 晚餐
    QString dishRole;        // breakfast / staple / meat / vegetable / soup / mixed
    QString steps;
    int cookMinutes = 0;
    QString accent;
    double totalCalories = 0.0;
    double totalProtein = 0.0;
    double totalCarbs = 0.0;
    double totalFat = 0.0;
    double totalWeight = 0.0;
    double per100Calories = 0.0;
    double per100Protein = 0.0;
    double per100Carbs = 0.0;
    double per100Fat = 0.0;
    QList<RecipeIngredient> ingredients;

    bool isValid() const { return id > 0; }
};

Q_DECLARE_METATYPE(Recipe)

#endif // RECIPE_H
