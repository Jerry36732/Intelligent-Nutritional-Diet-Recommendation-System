#pragma once

#include "../entities/Food.h"

#include <QDialog>

class QPushButton;
class QShowEvent;

class FoodDetailDialog final : public QDialog
{
    Q_OBJECT

public:
    FoodDetailDialog(const Food &food, const QString &category, int userId,
                     QWidget *parent = nullptr, int reviewFavoriteState = -1);

signals:
    void favoriteChanged(int foodId, bool favorite);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void refreshFavoriteButton();

    Food m_food;
    QString m_category;
    int m_userId = 0;
    bool m_favorite = false;
    int m_reviewFavoriteState = -1;
    QPushButton *m_favoriteButton = nullptr;
};
