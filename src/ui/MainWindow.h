#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "../entities/RecommendResult.h"
#include "../entities/User.h"
#include "../services/UserService.h"

#include <QMainWindow>
#include <QVector>

class QLabel;
class QPushButton;
class QStackedWidget;
class QButtonGroup;
class QFrame;
class QEvent;
class DashboardWidget;
class FoodSearchWidget;
class RecommendWidget;
class ProfileWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    /** 登录成功后进入主界面：写入用户、生成方案、加载食材库 */
    void startWithUser(const User &user);

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
    bool openDatabase();
    bool ensureLoggedIn();
    void applyUser(const User &user);
    void updateChrome();
    void setDbConnected(bool ok);

    UserService m_userService;
    User m_user;
    RecommendResult m_plan;

    QStackedWidget *m_stack = nullptr;
    QButtonGroup *m_navGroup = nullptr;
    QVector<QPushButton *> m_navButtons;

    QLabel *m_topDate = nullptr;
    QLabel *m_topTitle = nullptr;
    QLabel *m_dbBadge = nullptr;
    QLabel *m_profileName = nullptr;
    QLabel *m_profileMeta = nullptr;
    QFrame *m_profileMini = nullptr;
    QWidget *m_brandRow = nullptr;

    DashboardWidget *m_dashboard = nullptr;
    FoodSearchWidget *m_foods = nullptr;
    RecommendWidget *m_recommend = nullptr;
    ProfileWidget *m_profile = nullptr;
};

#endif // MAINWINDOW_H
