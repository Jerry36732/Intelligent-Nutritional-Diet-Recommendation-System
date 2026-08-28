#ifndef RECIPECARD_H
#define RECIPECARD_H

#include "../entities/Recipe.h"
#include "../entities/RecommendResult.h"

#include <QFrame>

class QLabel;
class QPushButton;

class RecipeCard : public QFrame
{
    Q_OBJECT

public:
    explicit RecipeCard(QWidget *parent = nullptr);

    void setRecipe(const Recipe &recipe);
    void setMeal(const MealSlot &meal);
    Recipe recipe() const;
    MealSlot meal() const;
    void setFavorited(bool favorited);
    bool isFavorited() const;
    void clear();

signals:
    void detailClicked(const Recipe &recipe);
    void mealDetailRequested(const MealSlot &meal);
    void favoriteToggled(int recipeId);

private:
    void rebuildNutrients();
    void applyMealUi();

    MealSlot m_meal;
    QLabel *m_mealTag = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_metaLabel = nullptr;
    QLabel *m_proteinChip = nullptr;
    QLabel *m_carbsChip = nullptr;
    QLabel *m_fatChip = nullptr;
    QPushButton *m_detailBtn = nullptr;
    QPushButton *m_favBtn = nullptr;
};

#endif // RECIPECARD_H
