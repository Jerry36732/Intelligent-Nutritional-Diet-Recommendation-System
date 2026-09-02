#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "../entities/RecommendResult.h"
#include "../entities/User.h"
#include "../services/UserService.h"

#include <QMainWindow>
#include <QPoint>
#include <QVector>

class QLabel;
class QPushButton;
class QStackedWidget;
class QButtonGroup;
class QFrame;
class QEvent;
class DashboardWidget;
class DietAnalyticsWidget;
class FoodSearchWidget;
class FavoritesWidget;
class FridgeWidget;
class RecommendWidget;
class ProfileWidget;
class RecipeLibraryWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    /** 登录成功后进入主界面：写入用户、生成方案、加载食材库 */
    void startWithUser(const User &user);
    void openFoodReviewDetail(bool favoriteState);
    void openFoodUsdaReview();
    void openRecipeReviewDetail(bool favoriteState, int recipeId = 469);
    void openFoodVisionReview();
    void openIngredientVisionReview();
    void openRecipeDnaReview(int recipeId = 469);
    void openFridgeVisionReview();
    void openFridgeVisionInitialReview();
    void openFridgeVisionFailureReview();
    void openRecommendPhotoReview();
    void runRecipeDnaCloseSmoke();
    void runFridgeVisionCloseSmoke();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onNavChanged(int index);
    void onGeneratePlan();
    void onShowDetail(const Recipe &recipe);
    void onShowMealDetail(const MealSlot &meal);
    void onFavoriteToggled(int recipeId);
    void onSwitchUser();
    void onOpenSettings();

private:
    enum PageIndex {
        TodayPage = 0,
        FoodPage,
        RecipeLibraryPage,
        FridgePage,
        RecommendPage,
        DietAnalyticsPage,
        FavoritesPage,
        ProfilePage
    };
    bool openDatabase();
    bool ensureLoggedIn();
    void applyUser(const User &user);
    void updateChrome();
    void setDbConnected(bool ok);
    void refreshFavoriteViews();

    UserService m_userService;
    User m_user;
    RecommendResult m_plan;

    QStackedWidget *m_stack = nullptr;
    QButtonGroup *m_navGroup = nullptr;
    QVector<QPushButton *> m_navButtons;

    QLabel *m_topDate = nullptr;
    QLabel *m_topTitle = nullptr;
    QLabel *m_topSubtitle = nullptr;
    QLabel *m_dbBadge = nullptr;
    QLabel *m_profileName = nullptr;
    QLabel *m_profileMeta = nullptr;
    QLabel *m_profileAvatar = nullptr;
    QWidget *m_recommendGoalTags = nullptr;
    QPushButton *m_recommendGoalTag = nullptr;
    QFrame *m_profileMini = nullptr;
    QFrame *m_sidebar = nullptr;
    QLabel *m_sidebarDecoration = nullptr;
    QLabel *m_topDecoration = nullptr;
    QFrame *m_titleBar = nullptr;
    QFrame *m_topBar = nullptr;
    QWidget *m_brandRow = nullptr;
    QPoint m_dragOffset;
    bool m_dragging = false;

    DashboardWidget *m_dashboard = nullptr;
    FoodSearchWidget *m_foods = nullptr;
    RecipeLibraryWidget *m_recipeLibrary = nullptr;
    RecommendWidget *m_recommend = nullptr;
    DietAnalyticsWidget *m_dietAnalytics = nullptr;
    FridgeWidget *m_fridge = nullptr;
    FavoritesWidget *m_favorites = nullptr;
    ProfileWidget *m_profile = nullptr;
};

#endif // MAINWINDOW_H
