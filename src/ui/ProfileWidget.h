#ifndef PROFILEWIDGET_H
#define PROFILEWIDGET_H

#include "../entities/Recipe.h"
#include "../entities/User.h"

#include <QWidget>

class QLabel;

class ProfileWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProfileWidget(QWidget *parent = nullptr);
    void setUser(const User &user);
    void reloadFavorites();

signals:
    void detailRequested(const Recipe &recipe);
    void favoriteToggled(int recipeId);
    void openSettingsRequested();
    void openFavoritesRequested();

private:
    void refreshHealthSummary();

    User m_user;
    QLabel *m_avatarLabel = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_goalChip = nullptr;
    QLabel *m_genderValue = nullptr;
    QLabel *m_heightValue = nullptr;
    QLabel *m_weightValue = nullptr;
    QLabel *m_bmiValue = nullptr;
    QLabel *m_calorieValue = nullptr;
    QLabel *m_proteinValue = nullptr;
    QLabel *m_preferenceValue = nullptr;
    QLabel *m_allergyValue = nullptr;
    QLabel *m_intoleranceValue = nullptr;
    QLabel *m_medicalValue = nullptr;
    QLabel *m_deficiencyValue = nullptr;
    QLabel *m_recipeFavoriteCount = nullptr;
    QLabel *m_foodFavoriteCount = nullptr;
};

#endif // PROFILEWIDGET_H
