#ifndef FOODVISIONDIALOG_H
#define FOODVISIONDIALOG_H

#include "../services/NutritionAiService.h"

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

class FoodVisionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FoodVisionDialog(int userId, QWidget *parent = nullptr);
    void setReviewState(const QString &imagePath);

signals:
    void foodLogSaved(int logId);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QDoubleSpinBox *createNumberField(const QString &unit, double maximum, int decimals);
    void chooseImage(bool sideView);
    void setImagePath(const QString &path, bool sideView = false);
    void analyze();
    void applyResult(const FoodVisionResult &result);
    void saveLog();
    void setBusy(bool busy);
    void refreshTodaySummary();
    void commitCurrentItem();
    void loadItem(int index);

    int m_userId = 0;
    QString m_topImagePath;
    QString m_sideImagePath;
    QString m_provider;
    double m_confidenceValue = 0.0;
    QList<FoodVisionItem> m_items;
    QStringList m_itemNotes;
    int m_currentItem = -1;
    bool m_loadingItem = false;
    NutritionAiService *m_service = nullptr;
    QLabel *m_topPreview = nullptr;
    QLabel *m_sidePreview = nullptr;
    QLabel *m_rangeLabel = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_confidence = nullptr;
    QLabel *m_todaySummary = nullptr;
    QLineEdit *m_name = nullptr;
    QComboBox *m_meal = nullptr;
    QComboBox *m_itemSelector = nullptr;
    QComboBox *m_reference = nullptr;
    QDoubleSpinBox *m_grams = nullptr;
    QDoubleSpinBox *m_calories = nullptr;
    QDoubleSpinBox *m_protein = nullptr;
    QDoubleSpinBox *m_carbs = nullptr;
    QDoubleSpinBox *m_fat = nullptr;
    QPlainTextEdit *m_notes = nullptr;
    QPushButton *m_analyze = nullptr;
    QPushButton *m_cancelRequest = nullptr;
    QPushButton *m_save = nullptr;
};

#endif // FOODVISIONDIALOG_H
