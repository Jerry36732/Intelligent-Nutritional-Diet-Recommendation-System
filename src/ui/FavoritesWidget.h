#ifndef FAVORITESWIDGET_H
#define FAVORITESWIDGET_H

#include "../entities/Recipe.h"
#include "../entities/User.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QTimer;
class QVBoxLayout;

class FavoritesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FavoritesWidget(QWidget *parent = nullptr);
    ~FavoritesWidget() override;
    void setUser(const User &user);
    void reload();

signals:
    void detailRequested(const Recipe &recipe);
    void favoriteToggled(int recipeId);
    void foodFavoriteToggled(int foodId);
    void personalRecipeCreated(int recipeId);

private slots:
    void openWebImporter();
    void openManualCreator();

private:
    void clearRows();

    User m_user;
    QLabel *m_recipeCount = nullptr;
    QLabel *m_foodCount = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QTimer *m_searchTimer = nullptr;
    QVBoxLayout *m_recipeRows = nullptr;
    QVBoxLayout *m_foodRows = nullptr;
};

#endif // FAVORITESWIDGET_H
