#ifndef FRIDGEWIDGET_H
#define FRIDGEWIDGET_H

#include "../entities/Recipe.h"
#include "../entities/RecommendResult.h"
#include "../entities/User.h"

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTableWidget;
class QWidget;

class FridgeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FridgeWidget(QWidget *parent = nullptr);
    void setUser(const User &user);
    void setPlan(const RecommendResult &plan);
    void reload();

signals:
    void detailRequested(const Recipe &recipe);
    void mealDetailRequested(const MealSlot &meal);
    void planGenerated(const RecommendResult &plan);

private slots:
    void onAdd();
    void onRemoveSelected();
    void onClearAll();
    void onRecommend();
    void onQuickAdd();
    void onSearchChanged();
    void onInventoryContextMenu(const QPoint &pos);
    void onResultClicked(QListWidgetItem *item);
    void onInventoryDoubleClicked(int row, int column);
    void onToggleView();
    void onShoppingList();
    void onRecommendExpiring();
    void onPhotoAdd();

private:
    void refreshInventory();
    void showDishRecommendations(const QStringList &fridgeFoods);
    void showDailyPlan(const RecommendResult &plan);
    void openRecipeById(int recipeId);
    void showAddEditor(bool visible);

    User m_user;
    QLineEdit *m_addEdit = nullptr;
    QDoubleSpinBox *m_qtySpin = nullptr;
    QDateEdit *m_expiryEdit = nullptr;
    QComboBox *m_unitCombo = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_statusFilter = nullptr;
    QTableWidget *m_inventoryTable = nullptr;
    QListWidget *m_resultList = nullptr;
    QWidget *m_addEditor = nullptr;
    QWidget *m_resultPanel = nullptr;
    QLabel *m_hintLabel = nullptr;
    QLabel *m_resultHint = nullptr;
    QLabel *m_kindValue = nullptr;
    QLabel *m_totalValue = nullptr;
    QLabel *m_expiringValue = nullptr;
    QLabel *m_expiredValue = nullptr;
    QPushButton *m_viewToggleBtn = nullptr;
    QWidget *m_expiryReminder = nullptr;
    QLabel *m_expiryReminderText = nullptr;
    RecommendResult m_plan;
    bool m_showingResults = false;
};

#endif // FRIDGEWIDGET_H
