#ifndef FRIDGEVISIONDIALOG_H
#define FRIDGEVISIONDIALOG_H

#include "../services/NutritionAiService.h"

#include <QDate>
#include <QDialog>

class QDateEdit;
class QComboBox;
class QCloseEvent;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;

class FridgeVisionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FridgeVisionDialog(int userId, QWidget *parent = nullptr);
    ~FridgeVisionDialog() override;
    static FridgeVisionDialog *create(int userId, QWidget *parent = nullptr);
    void setReviewState(const QString &imagePath = {});
    void setFailureReviewState();

public slots:
    void reject() override;

signals:
    void ingredientAdded();

private slots:
    void chooseImage();
    void applyResult(const FoodVisionResult &result);
    void setBusy(bool busy);
    void saveIngredient();
    void nextItem();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void showImage(const QString &path);
    void commitCurrentItem();
    void loadItem(int index);
    void setResultControlsEnabled(bool enabled);
    void updateNextButton();

    int m_userId = 0;
    NutritionAiService *m_service = nullptr;
    QString m_imagePath;
    QList<FoodVisionItem> m_items;
    QList<QDate> m_expiries;
    int m_currentItem = -1;
    bool m_loadingItem = false;
    QLabel *m_preview = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_confidence = nullptr;
    QLabel *m_details = nullptr;
    QLineEdit *m_name = nullptr;
    QComboBox *m_itemSelector = nullptr;
    QSlider *m_weightSlider = nullptr;
    QDoubleSpinBox *m_weightSpin = nullptr;
    QDateEdit *m_expiry = nullptr;
    QPushButton *m_choose = nullptr;
    QPushButton *m_next = nullptr;
    QPushButton *m_save = nullptr;
};

#endif // FRIDGEVISIONDIALOG_H
