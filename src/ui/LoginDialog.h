#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include "../entities/User.h"

#include <QDialog>
#include <QVector>

class QButtonGroup;
class QComboBox;
class QDate;
class QDoubleSpinBox;
class QEvent;
class QFrame;
class QLabel;
class QLineEdit;
class QObject;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QToolButton;
class TagChipGroup;

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);

    User user() const;
    /** 自动化视觉验收：0=登录，1..5=注册步骤。 */
    void setReviewState(int page);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void showLogin();
    void showRegister();
    void onLogin();
    void onRegisterFinish();
    void onWizardNext();
    void onWizardBack();

private:
    void setMode(bool registerMode);
    void setHint(const QString &text, bool error = false);
    void goRegisterStep(int step);
    bool validateRegisterStep(int step);
    QDate selectedBirthDate() const;
    void updateBirthDays();
    void updateWizardChrome();
    void updateStepRail();

    QStackedWidget *m_formStack = nullptr;
    QStackedWidget *m_wizardStack = nullptr;
    QPushButton *m_loginTab = nullptr;
    QPushButton *m_registerTab = nullptr;
    QFrame *m_shell = nullptr;
    QWidget *m_loginRail = nullptr;
    QWidget *m_registerRail = nullptr;
    QPushButton *m_backBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QPushButton *m_returnLoginBtn = nullptr;
    QWidget *m_navHost = nullptr;
    QVector<QFrame *> m_stepRows;
    QVector<QLabel *> m_stepNumbers;
    QVector<QLabel *> m_stepChecks;

    QLineEdit *m_loginNameEdit = nullptr;
    QLineEdit *m_loginPasswordEdit = nullptr;
    QLineEdit *m_registerNameEdit = nullptr;
    QLineEdit *m_registerPasswordEdit = nullptr;
    QLineEdit *m_registerPasswordConfirmEdit = nullptr;
    QComboBox *m_birthYearCombo = nullptr;
    QComboBox *m_birthMonthCombo = nullptr;
    QComboBox *m_birthDayCombo = nullptr;
    QComboBox *m_activityCombo = nullptr;
    QComboBox *m_genderCombo = nullptr;
    QComboBox *m_goalCombo = nullptr;
    QDoubleSpinBox *m_heightSpin = nullptr;
    QDoubleSpinBox *m_weightSpin = nullptr;
    QSpinBox *m_ageSpin = nullptr;
    QDoubleSpinBox *m_targetWeightSpin = nullptr;
    QSpinBox *m_planWeeksSpin = nullptr;
    QPushButton *m_maleRadio = nullptr;
    QPushButton *m_femaleRadio = nullptr;
    QButtonGroup *m_goalButtons = nullptr;

    TagChipGroup *m_dietGroup = nullptr;
    TagChipGroup *m_preferenceGroup = nullptr;
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
