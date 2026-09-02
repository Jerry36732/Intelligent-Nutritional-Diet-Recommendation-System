#ifndef RECIPECARD_H
#define RECIPECARD_H

#include "../entities/Recipe.h"
#include "../entities/RecommendResult.h"

#include <QFrame>
#include <QHash>

class QLabel;
class QPushButton;
class QGridLayout;
class QMouseEvent;
class QEvent;

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

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void rebuildDishes();
    void rebuildSummary();
    void applyMealUi();

    MealSlot m_meal;
    QLabel *m_mealTag = nullptr;
    QLabel *m_mealIcon = nullptr;
    QLabel *m_ratioLabel = nullptr;
    QLabel *m_totalKcalLabel = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QGridLayout *m_dishLayout = nullptr;
    QPushButton *m_detailBtn = nullptr;
    QPushButton *m_favBtn = nullptr;
    QHash<QObject *, Recipe> m_rowRecipes;
};

#endif // RECIPECARD_H
