#include "SettingsDialog.h"
#include "TagChipGroup.h"

#include "../services/UserService.h"

#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const User &user, QWidget *parent)
    : QDialog(parent)
    , m_user(user)
{
    setWindowTitle(QStringLiteral("健康档案 · %1").arg(user.name.isEmpty() ? QStringLiteral("用户") : user.name));
    setModal(true);
    resize(520, 680);
    setMinimumSize(480, 560);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 16);
    root->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("多维健康档案"), this);
    title->setObjectName(QStringLiteral("DialogTitle"));
    auto *subtitle = new QLabel(
        QStringLiteral("参考营养膳食推荐研究中的用户模型：饮食选择、不耐受、营养缺乏、过敏与医疗状况。"),
        this);
    subtitle->setWordWrap(true);
    subtitle->setProperty("class", QVariant(QStringLiteral("HintText")));

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *body = new QWidget;
    auto *bodyLay = new QVBoxLayout(body);
    bodyLay->setContentsMargins(0, 0, 8, 0);
    bodyLay->setSpacing(16);

    auto *basicTitle = new QLabel(QStringLiteral("基础信息"), body);
    basicTitle->setObjectName(QStringLiteral("TagGroupTitle"));
    auto *form = new QFormLayout;
    form->setSpacing(12);

    m_heightSpin = new QDoubleSpinBox(body);
    m_heightSpin->setRange(100.0, 250.0);
    m_heightSpin->setDecimals(1);
    m_heightSpin->setValue(user.height > 0 ? user.height : 170.0);
    m_heightSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    auto *heightWrap = new QWidget(body);
    auto *heightLay = new QHBoxLayout(heightWrap);
    heightLay->setContentsMargins(0, 0, 0, 0);
    heightLay->addWidget(m_heightSpin);
    heightLay->addWidget(new QLabel(QStringLiteral("cm"), heightWrap));

    m_weightSpin = new QDoubleSpinBox(body);
    m_weightSpin->setRange(30.0, 200.0);
    m_weightSpin->setDecimals(1);
    m_weightSpin->setValue(user.weight > 0 ? user.weight : 65.0);
    m_weightSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    auto *weightWrap = new QWidget(body);
    auto *weightLay = new QHBoxLayout(weightWrap);
    weightLay->setContentsMargins(0, 0, 0, 0);
    weightLay->addWidget(m_weightSpin);
    weightLay->addWidget(new QLabel(QStringLiteral("kg"), weightWrap));

    m_genderCombo = new QComboBox(body);
    m_genderCombo->addItem(QStringLiteral("男"), QStringLiteral("male"));
    m_genderCombo->addItem(QStringLiteral("女"), QStringLiteral("female"));
    m_genderCombo->setCurrentIndex(user.gender.compare(QStringLiteral("female"), Qt::CaseInsensitive) == 0 ? 1 : 0);

    m_goalCombo = new QComboBox(body);
    m_goalCombo->addItem(QStringLiteral("减重"), QStringLiteral("lose"));
    m_goalCombo->addItem(QStringLiteral("增肌"), QStringLiteral("gain"));
    m_goalCombo->addItem(QStringLiteral("维持"), QStringLiteral("maintain"));
    {
        const QString g = user.goal.toLower();
        if (g == QLatin1String("lose"))
            m_goalCombo->setCurrentIndex(0);
        else if (g == QLatin1String("gain"))
            m_goalCombo->setCurrentIndex(1);
        else
            m_goalCombo->setCurrentIndex(2);
    }

    m_prefEdit = new QPlainTextEdit(body);
    m_prefEdit->setPlaceholderText(QStringLiteral("例如：清淡、少油、高蛋白（可与下方标签互补）"));
    m_prefEdit->setPlainText(user.preferences);
    m_prefEdit->setFixedHeight(56);

    form->addRow(QStringLiteral("身高"), heightWrap);
    form->addRow(QStringLiteral("体重"), weightWrap);
    form->addRow(QStringLiteral("性别"), m_genderCombo);
    form->addRow(QStringLiteral("目标"), m_goalCombo);
    form->addRow(QStringLiteral("口味备注"), m_prefEdit);

    m_dietGroup = new TagChipGroup(
        QStringLiteral("饮食选择"),
        {QStringLiteral("荤食者"), QStringLiteral("素食者"), QStringLiteral("蛋奶素食者"),
         QStringLiteral("严格素食者"), QStringLiteral("清真"), QStringLiteral("避免红肉")},
        body);
    m_dietGroup->setSelected(user.dietaryChoices);

    m_intoleranceGroup = new TagChipGroup(
        QStringLiteral("食物不耐受"),
        {QStringLiteral("乳糖"), QStringLiteral("麸质"), QStringLiteral("果糖"),
         QStringLiteral("水杨酸盐"), QStringLiteral("亚硫酸盐"), QStringLiteral("FODMAPs")},
        body);
    m_intoleranceGroup->setSelected(user.foodIntolerances);

    m_deficiencyGroup = new TagChipGroup(
        QStringLiteral("营养缺乏 / 补充目标"),
        {QStringLiteral("缺铁"), QStringLiteral("缺钙"), QStringLiteral("缺维生素D"),
         QStringLiteral("缺维生素B12"), QStringLiteral("蛋白质不足")},
        body);
    m_deficiencyGroup->setSelected(user.nutritionalDeficiencies);

    m_allergyGroup = new TagChipGroup(
        QStringLiteral("过敏史"),
        {QStringLiteral("花生"), QStringLiteral("坚果"), QStringLiteral("鸡蛋"), QStringLiteral("牛奶"),
         QStringLiteral("海鲜"), QStringLiteral("贝类"), QStringLiteral("大豆"),
         QStringLiteral("麸质（小麦）"), QStringLiteral("芝麻"), QStringLiteral("猕猴桃")},
        body);
    m_allergyGroup->setSelected(user.allergies.isEmpty() ? User::splitLegacyText(user.allergens)
                                                         : user.allergies);

    m_medicalGroup = new TagChipGroup(
        QStringLiteral("医疗状况"),
        {QStringLiteral("2型糖尿病"), QStringLiteral("高血压"), QStringLiteral("高血脂"),
         QStringLiteral("心血管疾病"), QStringLiteral("肥胖"), QStringLiteral("贫血"),
         QStringLiteral("肾病")},
        body);
    m_medicalGroup->setSelected(user.medicalConditions);

    bodyLay->addWidget(basicTitle);
    bodyLay->addLayout(form);
    bodyLay->addWidget(m_dietGroup);
    bodyLay->addWidget(m_intoleranceGroup);
    bodyLay->addWidget(m_deficiencyGroup);
    bodyLay->addWidget(m_allergyGroup);
    bodyLay->addWidget(m_medicalGroup);
    bodyLay->addStretch();
    scroll->setWidget(body);

    auto *hint = new QLabel(
        QStringLiteral("保存后将重算热量目标；推荐会避开过敏/不耐受关键词，并参考饮食选择与营养目标。"),
        this);
    hint->setProperty("class", QVariant(QStringLiteral("HintText")));
    hint->setWordWrap(true);

    auto *buttons = new QDialogButtonBox(this);
    auto *saveBtn = buttons->addButton(QStringLiteral("保存"), QDialogButtonBox::AcceptRole);
    auto *cancelBtn = buttons->addButton(QStringLiteral("取消"), QDialogButtonBox::RejectRole);
    saveBtn->setProperty("class", QVariant(QStringLiteral("PrimaryButton")));
    cancelBtn->setProperty("class", QVariant(QStringLiteral("GhostButton")));

    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onSave);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    root->addWidget(title);
    root->addWidget(subtitle);
    root->addWidget(scroll, 1);
    root->addWidget(hint);
    root->addWidget(buttons);
}

User SettingsDialog::user() const
{
    return m_user;
}

void SettingsDialog::onSave()
{
    m_user.height = m_heightSpin->value();
    m_user.weight = m_weightSpin->value();
    m_user.gender = m_genderCombo->currentData().toString();
    m_user.goal = m_goalCombo->currentData().toString();
    m_user.preferences = m_prefEdit->toPlainText().trimmed();
    m_user.dietaryChoices = m_dietGroup->selected();
    m_user.foodIntolerances = m_intoleranceGroup->selected();
    m_user.nutritionalDeficiencies = m_deficiencyGroup->selected();
    m_user.allergies = m_allergyGroup->selected();
    m_user.medicalConditions = m_medicalGroup->selected();
    m_user.syncAllergenFields();

    UserService svc;
    if (!svc.saveUserProfile(m_user)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
                             QStringLiteral("无法更新用户档案，请稍后重试。"));
        return;
    }
    accept();
}
