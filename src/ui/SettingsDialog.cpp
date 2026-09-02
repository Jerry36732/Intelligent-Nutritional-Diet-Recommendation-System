#include "SettingsDialog.h"
#include "HealthProfileOptions.h"
#include "TagChipGroup.h"
#include "UiAssets.h"

#include "../services/UserService.h"

#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

namespace {
QString joinOrEmpty(const QStringList &values, const QString &empty)
{
    return values.isEmpty() ? empty : values.join(QStringLiteral("、"));
}

void editHealthTags(QWidget *parent, const QString &title, const QStringList &options,
                    TagChipGroup *storage, QLabel *summary, const QString &emptyText)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setModal(true);
    dialog.setObjectName(QStringLiteral("HealthTagEditor"));
    dialog.resize(430, 320);
    auto *root = new QVBoxLayout(&dialog);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(14);
    auto *caption = new QLabel(title, &dialog);
    caption->setObjectName(QStringLiteral("DialogTitle"));
    caption->setFont(UiAssets::titleFont(21));
    auto *editor = new TagChipGroup(title, options, &dialog, 6, true);
    editor->setSelected(storage->selected());
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save,
                                         &dialog);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    buttons->button(QDialogButtonBox::Save)->setProperty("class", QStringLiteral("PrimaryButton"));
    buttons->button(QDialogButtonBox::Cancel)->setProperty("class", QStringLiteral("GhostButton"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    root->addWidget(caption);
    root->addWidget(editor, 1);
    root->addWidget(buttons);
    if (dialog.exec() == QDialog::Accepted) {
        storage->setSelected(editor->selected());
        summary->setText(joinOrEmpty(storage->selected(), emptyText));
    }
}

QWidget *fieldBlock(QWidget *parent, const QString &labelText, QWidget *field)
{
    auto *host = new QWidget(parent);
    host->setObjectName(QStringLiteral("SettingsFieldBlock"));
    auto *layout = new QVBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    auto *label = new QLabel(labelText, host);
    label->setObjectName(QStringLiteral("SettingsFieldLabel"));
    field->setParent(host);
    // QSS 的水平/垂直内边距也计入最终尺寸；26 logical px 对应目标图约 34 px 外框。
    field->setFixedHeight(44);
    layout->addWidget(label);
    layout->addWidget(field);
    return host;
}
} // namespace

SettingsDialog::SettingsDialog(const User &user, QWidget *parent)
    : QDialog(parent)
    , m_user(user)
{
    setWindowTitle(QStringLiteral("健康档案 · %1").arg(
        user.name.isEmpty() ? QStringLiteral("用户") : user.name));
    setModal(true);
    setWindowFlag(Qt::FramelessWindowHint, true);
    setObjectName(QStringLiteral("SettingsDialog"));
    setFixedSize(620, 690);

    auto *root = new QVBoxLayout(this);
    root->setSizeConstraint(QLayout::SetNoConstraint);
    root->setContentsMargins(30, 22, 30, 28);
    root->setSpacing(14);

    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("SettingsHeader"));
    header->setFixedHeight(54);
    auto *headerLay = new QHBoxLayout(header);
    headerLay->setContentsMargins(0, 0, 0, 0);
    auto *title = new QLabel(QStringLiteral("设置"), header);
    title->setObjectName(QStringLiteral("DialogTitle"));
    title->setFont(UiAssets::titleFont(28));
    title->setAlignment(Qt::AlignCenter);
    auto *closeBtn = new QPushButton(header);
    closeBtn->setObjectName(QStringLiteral("DialogCloseButton"));
    closeBtn->setFixedSize(36, 36);
    UiAssets::setButtonIcon(closeBtn, QStringLiteral("close"), 20,
                            QColor(QStringLiteral("#FFFFFF")));
    headerLay->addSpacing(28);
    headerLay->addStretch();
    headerLay->addWidget(title);
    headerLay->addStretch();
    headerLay->addWidget(closeBtn);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    auto *body = new QWidget(this);
    body->setObjectName(QStringLiteral("SettingsBody"));
    auto *bodyLay = new QVBoxLayout(body);
    bodyLay->setContentsMargins(2, 0, 2, 0);
    bodyLay->setSpacing(9);

    auto *basicTitle = new QLabel(QStringLiteral("基本资料"), body);
    basicTitle->setObjectName(QStringLiteral("SettingsSectionTitle"));
    bodyLay->addWidget(basicTitle);

    auto *nameEdit = new QLineEdit(user.name, body);
    nameEdit->setReadOnly(true);
    m_genderCombo = new QComboBox(body);
    m_genderCombo->addItem(QStringLiteral("男"), QStringLiteral("male"));
    m_genderCombo->addItem(QStringLiteral("女"), QStringLiteral("female"));
    m_genderCombo->setCurrentIndex(
        user.gender.compare(QStringLiteral("female"), Qt::CaseInsensitive) == 0 ? 1 : 0);

    m_heightSpin = new QDoubleSpinBox(body);
    m_heightSpin->setRange(100.0, 250.0);
    m_heightSpin->setDecimals(1);
    m_heightSpin->setValue(user.height > 0 ? user.height : 170.0);
    m_heightSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    UiAssets::attachFixedUnit(m_heightSpin, QStringLiteral("cm"));

    m_weightSpin = new QDoubleSpinBox(body);
    m_weightSpin->setRange(30.0, 200.0);
    m_weightSpin->setDecimals(1);
    m_weightSpin->setValue(user.weight > 0 ? user.weight : 65.0);
    m_weightSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    UiAssets::attachFixedUnit(m_weightSpin, QStringLiteral("kg"));

    m_goalCombo = new QComboBox(body);
    m_goalCombo->addItem(QStringLiteral("减重"), QStringLiteral("lose"));
    m_goalCombo->addItem(QStringLiteral("增肌"), QStringLiteral("gain"));
    m_goalCombo->addItem(QStringLiteral("维持"), QStringLiteral("maintain"));
    const QString goal = user.goal.toLower();
    m_goalCombo->setCurrentIndex(goal == QLatin1String("lose") ? 0
                                 : goal == QLatin1String("gain") ? 1 : 2);
    auto *calorieEdit = new QLineEdit(
        QStringLiteral("%1 kcal").arg(user.calorieTarget > 0 ? user.calorieTarget : 2100), body);
    calorieEdit->setReadOnly(true);

    auto *formGrid = new QGridLayout;
    formGrid->setContentsMargins(0, 0, 0, 0);
    formGrid->setHorizontalSpacing(22);
    formGrid->setVerticalSpacing(8);
    formGrid->addWidget(fieldBlock(body, QStringLiteral("用户名"), nameEdit), 0, 0);
    formGrid->addWidget(fieldBlock(body, QStringLiteral("性别"), m_genderCombo), 0, 1);
    formGrid->addWidget(fieldBlock(body, QStringLiteral("身高"), m_heightSpin), 1, 0);
    formGrid->addWidget(fieldBlock(body, QStringLiteral("体重"), m_weightSpin), 1, 1);
    formGrid->addWidget(fieldBlock(body, QStringLiteral("健康目标"), m_goalCombo), 2, 0);
    formGrid->addWidget(fieldBlock(body, QStringLiteral("每日热量目标"), calorieEdit), 2, 1);
    formGrid->setColumnStretch(0, 1);
    formGrid->setColumnStretch(1, 1);
    bodyLay->addLayout(formGrid);

    m_prefEdit = new QPlainTextEdit(body);
    m_prefEdit->setPlainText(user.preferences);
    m_prefEdit->hide();
    m_dietGroup = new TagChipGroup(QStringLiteral("饮食选择"),
                                   HealthProfileOptions::dietaryChoices(), body, 6, true);
    m_dietGroup->setSelected(user.dietaryChoices);
    m_intoleranceGroup = new TagChipGroup(QStringLiteral("食物不耐受"),
                                          HealthProfileOptions::foodIntolerances(), body, 6, true);
    m_intoleranceGroup->setSelected(user.foodIntolerances);
    m_deficiencyGroup = new TagChipGroup(QStringLiteral("营养缺乏"),
                                         HealthProfileOptions::nutritionalDeficiencies(), body, 6,
                                         true);
    m_deficiencyGroup->setSelected(user.nutritionalDeficiencies);
    m_allergyGroup = new TagChipGroup(QStringLiteral("过敏史"), HealthProfileOptions::allergies(),
                                      body, 9, true);
    m_allergyGroup->setSelected(user.allergies.isEmpty()
                                    ? User::splitLegacyText(user.allergens) : user.allergies);
    m_medicalGroup = new TagChipGroup(QStringLiteral("医疗状况"),
                                      HealthProfileOptions::medicalConditions(), body, 6, true);
    m_medicalGroup->setSelected(user.medicalConditions);
    for (TagChipGroup *group : {m_dietGroup, m_intoleranceGroup, m_deficiencyGroup,
                                m_allergyGroup, m_medicalGroup})
        group->hide();

    auto *healthTitle = new QLabel(QStringLiteral("健康状况"), body);
    healthTitle->setObjectName(QStringLiteral("SettingsSectionTitle"));
    bodyLay->addWidget(healthTitle);

    auto *healthGrid = new QGridLayout;
    healthGrid->setContentsMargins(0, 0, 0, 0);
    healthGrid->setHorizontalSpacing(16);
    healthGrid->setVerticalSpacing(14);
    auto addHealthCard = [this, body, healthGrid](int row, int column, const QString &caption,
                                                  const QString &iconName, const QString &tone,
                                                  TagChipGroup *storage,
                                                  const QStringList &options,
                                                  const QString &emptyText) {
        auto *card = new QFrame(body);
        card->setProperty("class", QStringLiteral("SettingsHealthTile"));
        card->setProperty("tone", tone);
        auto *layout = new QHBoxLayout(card);
        layout->setContentsMargins(15, 12, 12, 12);
        layout->setSpacing(12);
        const QColor color(tone == QStringLiteral("green") ? QStringLiteral("#08A96E")
                           : tone == QStringLiteral("orange") ? QStringLiteral("#D88931")
                           : tone == QStringLiteral("blue") ? QStringLiteral("#4B79D8")
                                                              : QStringLiteral("#735DD1"));
        auto *icon = UiAssets::createIconLabel(card, iconName, 30, color);
        auto *copy = new QVBoxLayout;
        copy->setSpacing(2);
        auto *titleLabel = new QLabel(caption, card);
        titleLabel->setObjectName(QStringLiteral("SettingsHealthCaption"));
        auto *summary = new QLabel(joinOrEmpty(storage->selected(), emptyText), card);
        summary->setObjectName(QStringLiteral("SettingsHealthValue"));
        summary->setWordWrap(true);
        copy->addWidget(titleLabel);
        copy->addWidget(summary);
        auto *edit = new QPushButton(card);
        edit->setProperty("class", QStringLiteral("SettingsHealthEdit"));
        edit->setFixedSize(38, 38);
        edit->setToolTip(QStringLiteral("编辑%1").arg(caption));
        UiAssets::setButtonIcon(edit, QStringLiteral("edit"), 17, color);
        layout->addWidget(icon);
        layout->addLayout(copy, 1);
        layout->addWidget(edit);
        healthGrid->addWidget(card, row, column);
        connect(edit, &QPushButton::clicked, this,
                [this, caption, options, storage, summary, emptyText]() {
            editHealthTags(this, caption, options, storage, summary, emptyText);
        });
    };
    addHealthCard(0, 0, QStringLiteral("过敏原"), QStringLiteral("shield"),
                  QStringLiteral("green"), m_allergyGroup, HealthProfileOptions::allergies(),
                  QStringLiteral("暂无记录"));
    addHealthCard(0, 1, QStringLiteral("食物不耐受"), QStringLiteral("stomach"),
                  QStringLiteral("orange"), m_intoleranceGroup,
                  HealthProfileOptions::foodIntolerances(), QStringLiteral("暂无记录"));
    addHealthCard(1, 0, QStringLiteral("医疗状况"), QStringLiteral("medical-heart"),
                  QStringLiteral("blue"), m_medicalGroup,
                  HealthProfileOptions::medicalConditions(), QStringLiteral("暂无记录"));
    addHealthCard(1, 1, QStringLiteral("营养缺乏"), QStringLiteral("vitamin"),
                  QStringLiteral("purple"), m_deficiencyGroup,
                  HealthProfileOptions::nutritionalDeficiencies(), QStringLiteral("暂无记录"));
    bodyLay->addLayout(healthGrid);

    auto *buttons = new QWidget(this);
    buttons->setObjectName(QStringLiteral("SettingsActions"));
    auto *buttonLay = new QHBoxLayout(buttons);
    buttonLay->setContentsMargins(0, 0, 0, 0);
    buttonLay->setSpacing(12);
    auto *cancelBtn = new QPushButton(QStringLiteral("取消"), buttons);
    auto *saveBtn = new QPushButton(QStringLiteral("保存设置"), buttons);
    cancelBtn->setProperty("class", QStringLiteral("GhostButton"));
    saveBtn->setProperty("class", QStringLiteral("PrimaryButton"));
    cancelBtn->setFixedSize(112, 48);
    saveBtn->setFixedSize(158, 48);
    buttonLay->addStretch();
    buttonLay->addWidget(cancelBtn);
    buttonLay->addWidget(saveBtn);
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::onSave);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    root->addWidget(header);
    root->addWidget(body, 1);
    root->addWidget(buttons);
}

User SettingsDialog::user() const
{
    return m_user;
}

void SettingsDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    setMinimumSize(620, 690);
    setMaximumSize(620, 690);
    resize(620, 690);
    if (QWidget *owner = parentWidget()) {
        const QRect parentRect = owner->frameGeometry();
        move(parentRect.center().x() - width() / 2,
             parentRect.center().y() - height() / 2);
    }
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
    UserService service;
    if (!service.saveUserProfile(m_user)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
                             QStringLiteral("无法更新用户档案，请稍后重试。"));
        return;
    }
    accept();
}
