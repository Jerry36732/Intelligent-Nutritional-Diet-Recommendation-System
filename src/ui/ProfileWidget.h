#ifndef PROFILEWIDGET_H
#define PROFILEWIDGET_H

#include "../entities/Recipe.h"
#include "../entities/User.h"

#include <QWidget>

class QLabel;
class QVBoxLayout;
class QWidget;

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

private:
    void clearFavoriteCards();
    void refreshHealthSummary();
    QWidget *makeDimensionRow(const QString &title, const QStringList &values);

    User m_user;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_heightValue = nullptr;
    QLabel *m_weightValue = nullptr;
    QLabel *m_bmiValue = nullptr;
    QLabel *m_bmiNote = nullptr;
    QLabel *m_goalValue = nullptr;
    QLabel *m_goalNote = nullptr;
    QLabel *m_favCount = nullptr;
    QVBoxLayout *m_favLayout = nullptr;
    QVBoxLayout *m_healthLayout = nullptr;
};

#endif // PROFILEWIDGET_H
