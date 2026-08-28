#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include "../entities/User.h"

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QStackedWidget;
class TagChipGroup;

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);

    User user() const;

private slots:
    void showLogin();
    void showRegister();
    void onLogin();
    void onRegisterFinish();
    void onWizardNext();
    void onWizardBack();
    void syncHeightFromSlider(int value);
    void syncHeightFromSpin(double value);
    void syncWeightFromSlider(int value);
    void syncWeightFromSpin(double value);

private:
    void setMode(bool registerMode);
    void setHint(const QString &text, bool error = false);
    void goRegisterStep(int step);
    bool validateRegisterStep(int step);
    void updateWizardChrome();

    QStackedWidget *m_formStack = nullptr;
    QStackedWidget *m_wizardStack = nullptr;
    QPushButton *m_loginTab = nullptr;
    QPushButton *m_registerTab = nullptr;
    QLabel *m_stepLabel = nullptr;
    QPushButton *m_backBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;

    QLineEdit *m_loginNameEdit = nullptr;
    QLineEdit *m_loginPasswordEdit = nullptr;
    QLineEdit *m_registerNameEdit = nullptr;
    QLineEdit *m_registerPasswordEdit = nullptr;
    QLineEdit *m_registerPasswordConfirmEdit = nullptr;
    QComboBox *m_genderCombo = nullptr;
    QComboBox *m_goalCombo = nullptr;
    QSlider *m_heightSlider = nullptr;
    QSlider *m_weightSlider = nullptr;
    QDoubleSpinBox *m_heightSpin = nullptr;
    QDoubleSpinBox *m_weightSpin = nullptr;

    TagChipGroup *m_dietGroup = nullptr;
    TagChipGroup *m_allergyGroup = nullptr;
    TagChipGroup *m_intoleranceGroup = nullptr;
    TagChipGroup *m_deficiencyGroup = nullptr;
    TagChipGroup *m_medicalGroup = nullptr;

    QLabel *m_hintLabel = nullptr;
    int m_regStep = 0;
    static constexpr int kRegSteps = 5;
    User m_user;
};

#endif // LOGINDIALOG_H
