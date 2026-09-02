#ifndef RECIPEDNADIALOG_H
#define RECIPEDNADIALOG_H

#include "../entities/Recipe.h"
#include "../services/NutritionAiService.h"
#include "../services/FlavorFingerprintService.h"

#include <QDialog>

class QLabel;
class QHBoxLayout;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;
class FlavorRadarWidget;

class RecipeDnaDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RecipeDnaDialog(const Recipe &recipe, int userId, QWidget *parent = nullptr);
    void setReviewState();
    void requestTransform(const QString &instruction);

public slots:
    void accept() override;
    void reject() override;

signals:
    void personalRecipeCreated(int recipeId);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void runTransform();
    void applyResult(const RecipeDnaResult &result);
    void saveRecipe();
    void setBusy(bool busy);
    void addPreset(const QString &text, const QString &instruction, QHBoxLayout *layout);
    void shutdownRequest();

    Recipe m_recipe;
    int m_userId = 0;
    NutritionAiService *m_service = nullptr;
    RecipeDnaResult m_result;
    FlavorFingerprint m_originalFlavor;
    FlavorFingerprint m_transformedFlavor;
    QPlainTextEdit *m_instruction = nullptr;
    QLineEdit *m_name = nullptr;
    QTableWidget *m_ingredients = nullptr;
    QPlainTextEdit *m_steps = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_nutrition = nullptr;
    QLabel *m_summary = nullptr;
    QLabel *m_flavorSummary = nullptr;
    FlavorRadarWidget *m_flavorRadar = nullptr;
    QPushButton *m_generate = nullptr;
    QPushButton *m_cancelRequest = nullptr;
    QPushButton *m_save = nullptr;
    bool m_shuttingDown = false;
};

#endif // RECIPEDNADIALOG_H
