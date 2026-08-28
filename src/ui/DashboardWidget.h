#ifndef DASHBOARDWIDGET_H
#define DASHBOARDWIDGET_H

#include "../entities/RecommendResult.h"
#include "../entities/User.h"

#include <QWidget>

class QLabel;
class RecipeCard;

class DashboardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardWidget(QWidget *parent = nullptr);

    void setUser(const User &user);
    void setPlan(const RecommendResult &plan);
    void refreshFavorites(int userId);

signals:
    void regenerateRequested();
    void detailRequested(const Recipe &recipe);
    void mealDetailRequested(const MealSlot &meal);
    void favoriteToggled(int recipeId);
    void openSettingsRequested();

private:
    void updateMetrics();

    User m_user;
    RecommendResult m_plan;

    QLabel *m_welcomeLabel = nullptr;
    QLabel *m_kcalValue = nullptr;
    QLabel *m_goalValue = nullptr;
    QLabel *m_bmiValue = nullptr;
    QLabel *m_completionValue = nullptr;
    QLabel *m_summaryLabel = nullptr;
    RecipeCard *m_breakfastCard = nullptr;
    RecipeCard *m_lunchCard = nullptr;
    RecipeCard *m_dinnerCard = nullptr;
};

#endif // DASHBOARDWIDGET_H
