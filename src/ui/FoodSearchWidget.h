#ifndef FOODSEARCHWIDGET_H
#define FOODSEARCHWIDGET_H

#include "../entities/Food.h"

#include <QHash>
#include <QList>
#include <QWidget>

class QLineEdit;
class QTableWidget;
class QLabel;
class QPushButton;
class QTimer;

struct FoodRow {
    Food food;
    QString category;
    QString subcategory;
};

class FoodSearchWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FoodSearchWidget(QWidget *parent = nullptr);

    void reload();
    void setUserId(int userId);
    void openReviewDetail(bool favoriteState);
    void setUsdaReviewState();

signals:
    void foodFavoriteChanged();

private slots:
    void onSearchTextChanged(const QString &text);
    void onCategoryClicked();

private:
    void populateTable(const QList<const FoodRow *> &rows);
    void refreshResults();
    QString categoryForFood(const Food &food) const;
    QString subcategoryForCategory(const QString &category) const;
    void updateCategoryButtons();
    const FoodRow *foodRowById(int foodId) const;
    void openFoodDetail(const FoodRow &row, int reviewFavoriteState = -1);
    void openIngredientVision();

    QLineEdit *m_searchEdit = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_categoryCount = nullptr;
    QLabel *m_favoriteCount = nullptr;
    QList<FoodRow> m_rows;
    QList<QPushButton *> m_categoryButtons;
    QTimer *m_searchTimer = nullptr;
    QString m_selectedCategory = QStringLiteral("全部");
    bool m_loaded = false;
    bool m_refreshing = false;
    int m_userId = 0;
};

#endif // FOODSEARCHWIDGET_H
