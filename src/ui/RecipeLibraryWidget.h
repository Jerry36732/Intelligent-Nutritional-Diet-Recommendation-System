#ifndef RECIPELIBRARYWIDGET_H
#define RECIPELIBRARYWIDGET_H

#include "../entities/Recipe.h"
#include "../entities/User.h"

#include <QWidget>

class QButtonGroup;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;

class RecipeLibraryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RecipeLibraryWidget(QWidget *parent = nullptr);
    void setUser(const User &user);
    void reload();

signals:
    void detailRequested(const Recipe &recipe);
    void favoriteToggled(int recipeId);
    void personalRecipeCreated(int recipeId);

private slots:
    void openWebImporter();
    void openManualCreator();

private:
    void rebuild();
    void clearCards();
    QWidget *createRecipeCard(const Recipe &recipe);
    void setFilter(const QString &key);
    QString roleLabel(const Recipe &recipe) const;

    User m_user;
    QString m_filterKey;
    int m_page = 0;
    int m_total = 0;
    static constexpr int PageSize = 6;

    QLineEdit *m_search = nullptr;
    QLabel *m_count = nullptr;
    QGridLayout *m_grid = nullptr;
    QLabel *m_pageLabel = nullptr;
    QPushButton *m_prev = nullptr;
    QPushButton *m_next = nullptr;
    QButtonGroup *m_filters = nullptr;
    QTimer *m_searchTimer = nullptr;
};

#endif // RECIPELIBRARYWIDGET_H
