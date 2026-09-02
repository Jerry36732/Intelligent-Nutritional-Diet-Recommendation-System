#ifndef RECIPEDETAILDIALOG_H
#define RECIPEDETAILDIALOG_H

#include "../entities/Recipe.h"
#include "../entities/RecommendResult.h"

#include <QDialog>

class QShowEvent;

class RecipeDetailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RecipeDetailDialog(const Recipe &recipe, int userId, QWidget *parent = nullptr,
                                int reviewFavoriteState = -1);
    explicit RecipeDetailDialog(const MealSlot &meal, int userId, QWidget *parent = nullptr,
                                int reviewFavoriteState = -1);

signals:
    void favoriteChanged(int recipeId, bool favorited);
    void personalRecipeCreated(int recipeId);

private:
    void showEvent(QShowEvent *event) override;
    void initForMeal(const MealSlot &meal);
    int m_userId = 0;
    int m_reviewFavoriteState = -1;
};

#endif // RECIPEDETAILDIALOG_H
