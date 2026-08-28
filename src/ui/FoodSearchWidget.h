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

private slots:
    void onSearchTextChanged(const QString &text);
    void onCategoryClicked();

private:
    void populateTable(const QList<const FoodRow *> &rows);
    void refreshResults();
    QString categoryForFood(const Food &food) const;
    QString subcategoryForCategory(const QString &category) const;
    void updateCategoryButtons();

    QLineEdit *m_searchEdit = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_countLabel = nullptr;
    QList<FoodRow> m_rows;
    QList<QPushButton *> m_categoryButtons;
    QString m_selectedCategory = QStringLiteral("全部分类");
    bool m_loaded = false;
    bool m_refreshing = false;
};

#endif // FOODSEARCHWIDGET_H
