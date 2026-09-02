#ifndef INGREDIENTVISIONDIALOG_H
#define INGREDIENTVISIONDIALOG_H

#include "../services/NutritionAiService.h"

#include <QDialog>

class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

class IngredientVisionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit IngredientVisionDialog(int userId, QWidget *parent = nullptr);
    void setReviewState(const QString &imagePath = {});

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void chooseImage();
    void showImage(const QString &path);
    void applyResult(const FoodVisionResult &result);
    void setBusy(bool busy);
    void loadItem(int index);
    void refreshRecipes(const QString &ingredientName);
    void openSelectedRecipe();

    int m_userId = 0;
    QString m_imagePath;
    QList<FoodVisionItem> m_items;
    QList<int> m_recipeIds;
    NutritionAiService *m_service = nullptr;
    QLabel *m_preview = nullptr;
    QLabel *m_confidence = nullptr;
    QLabel *m_category = nullptr;
    QLabel *m_taste = nullptr;
    QLabel *m_uses = nullptr;
    QLabel *m_nutrition = nullptr;
    QLabel *m_status = nullptr;
    QLineEdit *m_name = nullptr;
    QComboBox *m_selector = nullptr;
    QListWidget *m_recipes = nullptr;
    QPushButton *m_choose = nullptr;
};

#endif // INGREDIENTVISIONDIALOG_H
