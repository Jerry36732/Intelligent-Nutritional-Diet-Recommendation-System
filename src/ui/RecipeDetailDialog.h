#ifndef RECIPEDETAILDIALOG_H
#define RECIPEDETAILDIALOG_H

#include "../entities/Recipe.h"
#include "../entities/RecommendResult.h"

#include <QDialog>

class RecipeDetailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RecipeDetailDialog(const Recipe &recipe, QWidget *parent = nullptr);
    explicit RecipeDetailDialog(const MealSlot &meal, QWidget *parent = nullptr);

private:
    void initForMeal(const MealSlot &meal);
};

#endif // RECIPEDETAILDIALOG_H
