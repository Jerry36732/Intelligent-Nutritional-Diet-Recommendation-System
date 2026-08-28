#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include "../entities/User.h"

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QPlainTextEdit;
class TagChipGroup;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const User &user, QWidget *parent = nullptr);

    User user() const;

private slots:
    void onSave();

private:
    User m_user;
    QDoubleSpinBox *m_heightSpin = nullptr;
    QDoubleSpinBox *m_weightSpin = nullptr;
    QComboBox *m_genderCombo = nullptr;
    QComboBox *m_goalCombo = nullptr;
    QPlainTextEdit *m_prefEdit = nullptr;

    TagChipGroup *m_dietGroup = nullptr;
    TagChipGroup *m_intoleranceGroup = nullptr;
    TagChipGroup *m_deficiencyGroup = nullptr;
    TagChipGroup *m_allergyGroup = nullptr;
    TagChipGroup *m_medicalGroup = nullptr;
};

#endif // SETTINGSDIALOG_H
