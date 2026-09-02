#include "LoginDialog.h"

#include "LogoWidget.h"
#include "TagChipGroup.h"
#include "UiAssets.h"

#include "../dao/DatabaseManager.h"
#include "../dao/UserDAO.h"
#include "../services/AuthUtils.h"
#include "../services/UserService.h"

#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QAction>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {
void startupTrace(const char *step)
{
    const QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/startup.log");
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        file.write(step);
        file.write("\n");
        file.flush();
    }
}

QLabel *fieldLabel(const QString &text, QWidget *parent, bool required = true)
{
    auto *label = new QLabel(parent);
    label->setProperty("class", QStringLiteral("LoginFieldLabel"));
    label->setTextFormat(Qt::RichText);
    label->setText(required
                       ? QStringLiteral("%1 <span style='color:#E43D45'>*</span>").arg(text)
                       : text);
    label->setFixedHeight(23);
    return label;
}

QLabel *copyLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setProperty("class", QStringLiteral("LoginFormCopy"));
    label->setWordWrap(true);
    return label;
}

QLabel *hintLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setProperty("class", QStringLiteral("RegisterFieldHint"));
    label->setWordWrap(true);
    return label;
}

QFrame *rule(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setObjectName(QStringLiteral("LoginRule"));
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    return line;
}

void enablePasswordReveal(QLineEdit *edit)
{
    auto *action = edit->addAction(UiAssets::svgIcon(QStringLiteral("eye")),
                                   QLineEdit::TrailingPosition);
    action->setToolTip(QStringLiteral("显示密码"));
    QObject::connect(action, &QAction::triggered, edit, [edit, action]() {
        const bool reveal = edit->echoMode() == QLineEdit::Password;
        edit->setEchoMode(reveal ? QLineEdit::Normal : QLineEdit::Password);
        action->setIcon(UiAssets::svgIcon(QStringLiteral("eye")));
        action->setToolTip(reveal ? QStringLiteral("隐藏密码") : QStringLiteral("显示密码"));
    });
}

QLineEdit *lineEdit(QWidget *parent, const QString &placeholder, bool password = false)
{
    auto *edit = new QLineEdit(parent);
    edit->setObjectName(QStringLiteral("LoginInput"));
    edit->setPlaceholderText(placeholder);
    edit->setFixedHeight(52);
    if (password) {
        edit->setEchoMode(QLineEdit::Password);
        enablePasswordReveal(edit);
    } else {
        edit->setClearButtonEnabled(true);
    }
    return edit;
}

void repolish(QWidget *widget)
{
    if (!widget)
        return;
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

void configureSpinBox(QAbstractSpinBox *spin)
{
    spin->setObjectName(QStringLiteral("LoginNumber"));
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setFixedHeight(47);
}
} // namespace

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    startupTrace("login_ctor_begin");
    setWindowTitle(QStringLiteral("膳衡 · 欢迎使用"));
    setModal(true);
    setWindowFlag(Qt::FramelessWindowHint, true);
    setFixedSize(1024, 768);
    setObjectName(QStringLiteral("LoginDialog"));

    auto *page = new QVBoxLayout(this);
    page->setContentsMargins(0, 0, 0, 0);
    page->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    m_shell = new QFrame(this);
    m_shell->setObjectName(QStringLiteral("LoginShell"));
    m_shell->setFixedSize(1024, 768);
    auto *shellLayout = new QHBoxLayout(m_shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    auto *railStack = new QStackedWidget(m_shell);
    railStack->setObjectName(QStringLiteral("LoginRailStack"));
    railStack->setFixedWidth(286);

    m_loginRail = new QWidget(railStack);
    m_loginRail->setObjectName(QStringLiteral("LoginRail"));
    auto *loginRailLayout = new QVBoxLayout(m_loginRail);
    loginRailLayout->setContentsMargins(0, 72, 28, 48);
    loginRailLayout->setSpacing(0);
    auto *loginLogo = new LogoWidget(m_loginRail);
    loginLogo->setFixedSize(250, 72);
    loginRailLayout->addWidget(loginLogo, 0, Qt::AlignLeft);
    loginRailLayout->addSpacing(39);
    auto *productCopy = new QLabel(QStringLiteral("智能营养膳食推荐系统"), m_loginRail);
    productCopy->setObjectName(QStringLiteral("LoginProductCopy"));
    productCopy->setContentsMargins(32, 0, 0, 0);
    loginRailLayout->addWidget(productCopy);
    loginRailLayout->addStretch();
    auto *privacyRow = new QHBoxLayout;
    privacyRow->setContentsMargins(31, 0, 0, 0);
    privacyRow->setSpacing(10);
    privacyRow->addWidget(UiAssets::createIconLabel(
        m_loginRail, QStringLiteral("shield"), 30, QColor(QStringLiteral("#0AA873"))),
        0, Qt::AlignTop);
    auto *privacy = new QLabel(
        QStringLiteral("我们重视你的隐私与数据安全。\n所有信息仅用于为你提供\n个性化的膳食建议。"),
        m_loginRail);
    privacy->setObjectName(QStringLiteral("LoginPrivacyCopy"));
    privacyRow->addWidget(privacy, 1);
    loginRailLayout->addLayout(privacyRow);
    loginRailLayout->addSpacing(118);

    m_registerRail = new QWidget(railStack);
    m_registerRail->setObjectName(QStringLiteral("RegisterRail"));
    auto *registerRailLayout = new QVBoxLayout(m_registerRail);
    registerRailLayout->setContentsMargins(35, 32, 35, 28);
    registerRailLayout->setSpacing(0);
    auto *registerLogo = new LogoWidget(m_registerRail);
    registerLogo->setFixedSize(230, 96);
    registerRailLayout->addWidget(registerLogo, 0, Qt::AlignHCenter);
    registerRailLayout->addSpacing(18);

    const QStringList stepNames = {QStringLiteral("创建账户"), QStringLiteral("基本信息"),
                                   QStringLiteral("健康状况"), QStringLiteral("饮食偏好"),
                                   QStringLiteral("目标设置")};
    for (int i = 0; i < stepNames.size(); ++i) {
        auto *row = new QFrame(m_registerRail);
        row->setObjectName(QStringLiteral("RegisterStepRow"));
        row->setFixedHeight(46);
        row->setFixedWidth(210);
        row->setProperty("state", QStringLiteral("pending"));
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(9, 5, 12, 5);
        rowLayout->setSpacing(10);
        auto *number = new QLabel(QString::number(i + 1), row);
        number->setObjectName(QStringLiteral("RegisterStepNumber"));
        number->setAlignment(Qt::AlignCenter);
        number->setFixedSize(34, 34);
        auto *name = new QLabel(stepNames.at(i), row);
        name->setObjectName(QStringLiteral("RegisterStepName"));
        auto *check = new QLabel(row);
        check->setObjectName(QStringLiteral("RegisterStepCheck"));
        check->setAlignment(Qt::AlignCenter);
        check->setFixedSize(20, 20);
        check->setPixmap(UiAssets::svgPixmap(QStringLiteral("check"), QSize(18, 18),
                                             QColor(QStringLiteral("#08A96E")), check));
        rowLayout->addWidget(number);
        rowLayout->addWidget(name, 1);
        rowLayout->addWidget(check);
        registerRailLayout->addWidget(row, 0, Qt::AlignHCenter);
        m_stepRows.append(row);
        m_stepNumbers.append(number);
        m_stepChecks.append(check);
        if (i + 1 < stepNames.size()) {
            auto *connectorHost = new QWidget(m_registerRail);
            connectorHost->setFixedHeight(10);
            auto *connector = new QFrame(connectorHost);
            connector->setObjectName(QStringLiteral("RegisterStepConnector"));
            connector->setGeometry(29, 0, 1, 10);
            registerRailLayout->addWidget(connectorHost);
        }
    }
    registerRailLayout->addStretch();
    registerRailLayout->addWidget(rule(m_registerRail));
    registerRailLayout->addSpacing(24);
    auto *regPrivacyRow = new QHBoxLayout;
    regPrivacyRow->setSpacing(8);
    regPrivacyRow->addWidget(UiAssets::createIconLabel(
        m_registerRail, QStringLiteral("shield"), 28, QColor(QStringLiteral("#0AA873"))),
        0, Qt::AlignTop);
    auto *regPrivacy = new QLabel(
        QStringLiteral("我们重视你的隐私与数据安全。\n所有信息仅用于为你提供\n个性化的膳食建议。"),
        m_registerRail);
    regPrivacy->setObjectName(QStringLiteral("LoginPrivacyCopy"));
    regPrivacyRow->addWidget(regPrivacy, 1);
    registerRailLayout->addLayout(regPrivacyRow);
    registerRailLayout->addSpacing(24);
    m_returnLoginBtn = new QPushButton(QStringLiteral("返回登录"), m_registerRail);
    m_returnLoginBtn->setObjectName(QStringLiteral("RegisterReturnLogin"));
    UiAssets::setButtonIcon(m_returnLoginBtn, QStringLiteral("arrow-left"), 18,
                            QColor(QStringLiteral("#08A96E")));
    m_returnLoginBtn->setCursor(Qt::PointingHandCursor);
    registerRailLayout->addWidget(m_returnLoginBtn, 0, Qt::AlignLeft);

    railStack->addWidget(m_loginRail);
    railStack->addWidget(m_registerRail);

    m_formStack = new QStackedWidget(m_shell);
    m_formStack->setObjectName(QStringLiteral("LoginFormStack"));

    auto *loginPage = new QWidget(m_formStack);
    loginPage->setObjectName(QStringLiteral("LoginFormPage"));
    auto *loginLayout = new QVBoxLayout(loginPage);
    loginLayout->setContentsMargins(70, 86, 72, 48);
    loginLayout->setSpacing(0);
    auto *loginTitle = new QLabel(QStringLiteral("欢迎登录"), loginPage);
    loginTitle->setObjectName(QStringLiteral("LoginFormTitle"));
    loginTitle->setFont(UiAssets::titleFont(27));
    auto *loginCopy = copyLabel(QStringLiteral("登录后继续你的个性化膳食计划"), loginPage);
    m_loginNameEdit = lineEdit(loginPage, QStringLiteral("请输入用户名"));
    m_loginPasswordEdit = lineEdit(loginPage, QStringLiteral("请输入密码"), true);
    auto *remember = new QCheckBox(QStringLiteral("记住我"), loginPage);
    remember->setObjectName(QStringLiteral("LoginRemember"));
    auto *forgot = new QPushButton(QStringLiteral("忘记密码"), loginPage);
    forgot->setObjectName(QStringLiteral("LoginTextLink"));
    auto *accountRow = new QHBoxLayout;
    accountRow->addWidget(remember);
    accountRow->addStretch();
    accountRow->addWidget(forgot);
    auto *loginBtn = new QPushButton(QStringLiteral("开始健康之旅"), loginPage);
    loginBtn->setObjectName(QStringLiteral("LoginPrimaryAction"));
    loginBtn->setFont(UiAssets::bodyFont(17, QFont::Bold));
    loginBtn->setCursor(Qt::PointingHandCursor);
    loginBtn->setFixedHeight(58);
    auto *createRow = new QHBoxLayout;
    createRow->setSpacing(10);
    createRow->addStretch();
    auto *noAccount = new QLabel(QStringLiteral("还没有账号？"), loginPage);
    noAccount->setObjectName(QStringLiteral("LoginCreateCopy"));
    m_registerTab = new QPushButton(QStringLiteral("创建账户"), loginPage);
    m_registerTab->setObjectName(QStringLiteral("LoginCreateLink"));
    m_registerTab->setCursor(Qt::PointingHandCursor);
    createRow->addWidget(noAccount);
    createRow->addWidget(m_registerTab);
    createRow->addStretch();
    loginLayout->addWidget(loginTitle);
    loginLayout->addSpacing(20);
    loginLayout->addWidget(loginCopy);
    loginLayout->addSpacing(54);
    loginLayout->addWidget(fieldLabel(QStringLiteral("用户名"), loginPage));
    loginLayout->addSpacing(10);
    loginLayout->addWidget(m_loginNameEdit);
    loginLayout->addSpacing(30);
    loginLayout->addWidget(fieldLabel(QStringLiteral("密码"), loginPage));
    loginLayout->addSpacing(10);
    loginLayout->addWidget(m_loginPasswordEdit);
    loginLayout->addSpacing(17);
    loginLayout->addLayout(accountRow);
    loginLayout->addSpacing(29);
    loginLayout->addWidget(loginBtn);
    loginLayout->addSpacing(28);
    loginLayout->addWidget(rule(loginPage));
    loginLayout->addSpacing(23);
    loginLayout->addLayout(createRow);
    loginLayout->addStretch();

    auto *regRoot = new QWidget(m_formStack);
    regRoot->setObjectName(QStringLiteral("RegisterRoot"));
    auto *regLayout = new QVBoxLayout(regRoot);
    regLayout->setContentsMargins(66, 60, 66, 42);
    regLayout->setSpacing(0);
    m_wizardStack = new QStackedWidget(regRoot);
    m_wizardStack->setObjectName(QStringLiteral("RegisterWizardStack"));

    auto *p0 = new QWidget(m_wizardStack);
    auto *l0 = new QVBoxLayout(p0);
    l0->setContentsMargins(0, 0, 0, 0);
    l0->setSpacing(0);
    auto *t0 = new QLabel(QStringLiteral("创建你的膳食档案"), p0);
    t0->setObjectName(QStringLiteral("LoginFormTitle"));
    t0->setFont(UiAssets::titleFont(27));
    auto *c0 = copyLabel(QStringLiteral("请设置你的账户信息，用于登录和管理你的个性化膳食计划。"), p0);
    m_registerNameEdit = lineEdit(p0, QStringLiteral("请输入用户名"));
    m_registerPasswordEdit = lineEdit(p0, QStringLiteral("请输入密码"), true);
    m_registerPasswordConfirmEdit = lineEdit(p0, QStringLiteral("请再次输入密码"), true);
    l0->addWidget(t0);
    l0->addSpacing(8);
    l0->addWidget(c0);
    l0->addSpacing(42);
    l0->addWidget(fieldLabel(QStringLiteral("用户名"), p0));
    l0->addSpacing(8);
    l0->addWidget(m_registerNameEdit);
    l0->addSpacing(5);
    l0->addWidget(hintLabel(QStringLiteral("4-20个字符，支持中文、字母、数字和下划线"), p0));
    l0->addSpacing(27);
    l0->addWidget(fieldLabel(QStringLiteral("密码"), p0));
    l0->addSpacing(7);
    l0->addWidget(m_registerPasswordEdit);
    l0->addSpacing(5);
    l0->addWidget(hintLabel(QStringLiteral("8-20个字符，需包含字母和数字"), p0));
    l0->addSpacing(21);
    l0->addWidget(fieldLabel(QStringLiteral("确认密码"), p0));
    l0->addSpacing(12);
    l0->addWidget(m_registerPasswordConfirmEdit);
    l0->addSpacing(5);
    l0->addWidget(hintLabel(QStringLiteral("请确保两次输入的密码一致"), p0));
    l0->addStretch();

    auto *p1 = new QWidget(m_wizardStack);
    auto *l1 = new QVBoxLayout(p1);
    l1->setContentsMargins(0, 0, 0, 0);
    l1->setSpacing(0);
    auto *t1 = new QLabel(QStringLiteral("完善基础信息"), p1);
    t1->setObjectName(QStringLiteral("LoginFormTitle"));
    t1->setFont(UiAssets::titleFont(27));
    auto *c1 = copyLabel(QStringLiteral("这些信息将用于计算你的个性化营养需求。"), p1);
    auto *basicGrid = new QGridLayout;
    basicGrid->setContentsMargins(0, 0, 0, 0);
    basicGrid->setHorizontalSpacing(19);
    basicGrid->setVerticalSpacing(7);
    auto *genderHost = new QWidget(p1);
    auto *genderLayout = new QHBoxLayout(genderHost);
    genderLayout->setContentsMargins(0, 0, 0, 0);
    genderLayout->setSpacing(8);
    auto *genderButtons = new QButtonGroup(p1);
    genderButtons->setExclusive(true);
    m_maleRadio = new QPushButton(QStringLiteral("男"), genderHost);
    m_femaleRadio = new QPushButton(QStringLiteral("女"), genderHost);
    for (QPushButton *choice : {m_maleRadio, m_femaleRadio}) {
        choice->setProperty("class", QStringLiteral("GenderChoice"));
        choice->setCheckable(true);
        choice->setCursor(Qt::PointingHandCursor);
        choice->setFixedHeight(47);
        genderButtons->addButton(choice);
        genderLayout->addWidget(choice, 1);
    }
    m_genderCombo = new QComboBox(p1);
    m_genderCombo->addItem(QStringLiteral("男"), QStringLiteral("male"));
    m_genderCombo->addItem(QStringLiteral("女"), QStringLiteral("female"));
    m_genderCombo->setCurrentIndex(-1);
    m_genderCombo->hide();
    connect(genderButtons, &QButtonGroup::buttonClicked, this,
            [this](QAbstractButton *button) {
                m_genderCombo->setCurrentIndex(button == m_maleRadio ? 0
                                                : button == m_femaleRadio ? 1 : -1);
            });
    auto *birthHost = new QWidget(p1);
    birthHost->setObjectName(QStringLiteral("RegisterBirthDate"));
    auto *birthLayout = new QHBoxLayout(birthHost);
    birthLayout->setContentsMargins(0, 0, 0, 0);
    birthLayout->setSpacing(8);
    auto makeBirthCombo = [p1](const QString &placeholder) {
        auto *combo = new QComboBox(p1);
        combo->setObjectName(QStringLiteral("LoginBirthPart"));
        combo->setPlaceholderText(placeholder);
        combo->setCurrentIndex(-1);
        combo->setMaxVisibleItems(14);
        combo->setFixedHeight(47);
        return combo;
    };
    m_birthYearCombo = makeBirthCombo(QStringLiteral("年份"));
    m_birthMonthCombo = makeBirthCombo(QStringLiteral("月份"));
    m_birthDayCombo = makeBirthCombo(QStringLiteral("日期"));
    for (int year = QDate::currentDate().year(); year >= 1900; --year)
        m_birthYearCombo->addItem(QStringLiteral("%1年").arg(year), year);
    for (int month = 1; month <= 12; ++month)
        m_birthMonthCombo->addItem(QStringLiteral("%1月").arg(month), month);
    m_birthYearCombo->setCurrentIndex(-1);
    m_birthMonthCombo->setCurrentIndex(-1);
    updateBirthDays();
    birthLayout->addWidget(m_birthYearCombo, 5);
    birthLayout->addWidget(m_birthMonthCombo, 3);
    birthLayout->addWidget(m_birthDayCombo, 3);
    connect(m_birthYearCombo, &QComboBox::currentIndexChanged,
            this, &LoginDialog::updateBirthDays);
    connect(m_birthMonthCombo, &QComboBox::currentIndexChanged,
            this, &LoginDialog::updateBirthDays);
    m_heightSpin = new QDoubleSpinBox(p1);
    configureSpinBox(m_heightSpin);
    m_heightSpin->setRange(120, 230);
    m_heightSpin->setDecimals(0);
    m_heightSpin->setValue(120);
    m_heightSpin->setSpecialValueText(QStringLiteral("请输入身高"));
    UiAssets::attachFixedUnit(m_heightSpin, QStringLiteral("cm"));
    m_weightSpin = new QDoubleSpinBox(p1);
    configureSpinBox(m_weightSpin);
    m_weightSpin->setRange(30, 250);
    m_weightSpin->setDecimals(0);
    m_weightSpin->setValue(30);
    m_weightSpin->setSpecialValueText(QStringLiteral("请输入体重"));
    UiAssets::attachFixedUnit(m_weightSpin, QStringLiteral("kg"));
    m_activityCombo = new QComboBox(p1);
    m_activityCombo->setObjectName(QStringLiteral("LoginCombo"));
    m_activityCombo->setFixedHeight(47);
    m_activityCombo->addItem(QStringLiteral("请选择日常活动水平"), QString());
    m_activityCombo->addItem(QStringLiteral("久坐少动"), QStringLiteral("sedentary"));
    m_activityCombo->addItem(QStringLiteral("轻度活动"), QStringLiteral("light"));
    m_activityCombo->addItem(QStringLiteral("中度活动"), QStringLiteral("moderate"));
    m_activityCombo->addItem(QStringLiteral("高强度活动"), QStringLiteral("active"));
    m_ageSpin = new QSpinBox(p1);
    m_ageSpin->hide();
    basicGrid->addWidget(fieldLabel(QStringLiteral("性别"), p1), 0, 0);
    basicGrid->addWidget(fieldLabel(QStringLiteral("出生日期"), p1), 0, 1);
    basicGrid->addWidget(genderHost, 1, 0);
    basicGrid->addWidget(birthHost, 1, 1);
    basicGrid->addWidget(fieldLabel(QStringLiteral("身高"), p1), 3, 0);
    basicGrid->addWidget(fieldLabel(QStringLiteral("体重"), p1), 3, 1);
    basicGrid->addWidget(m_heightSpin, 4, 0);
    basicGrid->addWidget(m_weightSpin, 4, 1);
    basicGrid->addWidget(hintLabel(QStringLiteral("使用厘米（cm）作为单位"), p1), 5, 0);
    basicGrid->addWidget(hintLabel(QStringLiteral("使用千克（kg）作为单位"), p1), 5, 1);
    basicGrid->addWidget(fieldLabel(QStringLiteral("日常活动水平"), p1), 6, 0, 1, 2);
    basicGrid->addWidget(m_activityCombo, 7, 0, 1, 2);
    basicGrid->addWidget(hintLabel(QStringLiteral("根据你平时的活动量选择最符合的选项"), p1),
                         8, 0, 1, 2);
    l1->addWidget(t1);
    l1->addSpacing(8);
    l1->addWidget(c1);
    l1->addSpacing(28);
    l1->addLayout(basicGrid);
    l1->addSpacing(17);
    l1->addWidget(hintLabel(QStringLiteral("准确填写有助于我们为你提供更精准的营养建议。"), p1));
    l1->addStretch();

    auto *p2 = new QWidget(m_wizardStack);
    auto *l2 = new QVBoxLayout(p2);
    l2->setContentsMargins(0, 0, 0, 0);
    l2->setSpacing(0);
    auto *t2 = new QLabel(QStringLiteral("健康状况"), p2);
    t2->setObjectName(QStringLiteral("LoginFormTitle"));
    t2->setFont(UiAssets::titleFont(27));
    auto *c2 = copyLabel(QStringLiteral("用于规避风险并优化推荐；新用户默认不选择任何项目。"), p2);
    m_allergyGroup = new TagChipGroup(
        QStringLiteral("食物过敏原  （可多选）"),
        {QStringLiteral("花生"), QStringLiteral("坚果"), QStringLiteral("海鲜"),
         QStringLiteral("鸡蛋"), QStringLiteral("乳制品"), QStringLiteral("大豆"),
         QStringLiteral("小麦"), QStringLiteral("芝麻"), QStringLiteral("鱼类"),
         QStringLiteral("暂无 / 无相关情况")}, p2, 10, false);
    m_allergyGroup->setObjectName(QStringLiteral("RegisterV6Tags"));
    m_allergyGroup->setColumnCount(5);
    m_allergyGroup->setSelected({});
    m_intoleranceGroup = new TagChipGroup(
        QStringLiteral("食物不耐受  （可多选）"),
        {QStringLiteral("乳糖"), QStringLiteral("麸质"), QStringLiteral("果糖"),
         QStringLiteral("咖啡因"), QStringLiteral("辛辣食物"),
         QStringLiteral("暂无 / 无相关情况")}, p2, 6, false);
    m_intoleranceGroup->setObjectName(QStringLiteral("RegisterV6Tags"));
    m_intoleranceGroup->setColumnCount(5);
    m_intoleranceGroup->setSelected({});
    m_medicalGroup = new TagChipGroup(
        QStringLiteral("疾病与健康状况  （可多选）"),
        {QStringLiteral("糖尿病"), QStringLiteral("高血压"), QStringLiteral("高血脂"),
         QStringLiteral("痛风"), QStringLiteral("胃食管反流"), QStringLiteral("肝病"),
         QStringLiteral("肾病"), QStringLiteral("甲状腺疾病"), QStringLiteral("心血管疾病"),
         QStringLiteral("暂无 / 无相关情况")}, p2, 10, false);
    m_medicalGroup->setObjectName(QStringLiteral("RegisterV6Tags"));
    m_medicalGroup->setColumnCount(5);
    m_medicalGroup->setSelected({});
    m_deficiencyGroup = new TagChipGroup(QString(), {}, p2, 3, false);
    m_deficiencyGroup->hide();
    auto *healthNote = new QLabel(
        QStringLiteral("你的健康信息将严格保密，仅用于个性化膳食建议与风险规避。"), p2);
    healthNote->setObjectName(QStringLiteral("RegisterSecurityNote"));
    l2->addWidget(t2);
    l2->addSpacing(7);
    l2->addWidget(c2);
    l2->addSpacing(17);
    l2->addWidget(m_allergyGroup);
    l2->addSpacing(8);
    l2->addWidget(rule(p2));
    l2->addSpacing(8);
    l2->addWidget(m_intoleranceGroup);
    l2->addSpacing(8);
    l2->addWidget(rule(p2));
    l2->addSpacing(8);
    l2->addWidget(m_medicalGroup);
    l2->addSpacing(8);
    l2->addWidget(healthNote);
    l2->addStretch();

    auto *p3 = new QWidget(m_wizardStack);
    auto *l3 = new QVBoxLayout(p3);
    l3->setContentsMargins(0, 0, 0, 0);
    l3->setSpacing(0);
    auto *t3 = new QLabel(QStringLiteral("选择饮食偏好"), p3);
    t3->setObjectName(QStringLiteral("LoginFormTitle"));
    t3->setFont(UiAssets::titleFont(27));
    auto *c3 = copyLabel(QStringLiteral("帮助我们为你推荐更合口味的方案，稍后可以修改。"), p3);
    m_dietGroup = new TagChipGroup(
        QStringLiteral("饮食方式"),
        {QStringLiteral("均衡饮食"), QStringLiteral("素食"), QStringLiteral("低脂"),
         QStringLiteral("低碳水"), QStringLiteral("高蛋白")}, p3, 5, false);
    m_dietGroup->setObjectName(QStringLiteral("RegisterPreferenceTags"));
    m_dietGroup->setColumnCount(5);
    m_dietGroup->setSelected({});
    m_preferenceGroup = new TagChipGroup(
        QStringLiteral("口味偏好"),
        {QStringLiteral("清淡"), QStringLiteral("家常"), QStringLiteral("酸甜"),
         QStringLiteral("微辣"), QStringLiteral("不吃辣")}, p3, 5, false);
    m_preferenceGroup->setObjectName(QStringLiteral("RegisterPreferenceTags"));
    m_preferenceGroup->setColumnCount(5);
    m_preferenceGroup->setSelected({});
    l3->addWidget(t3);
    l3->addSpacing(7);
    l3->addWidget(c3);
    l3->addSpacing(33);
    l3->addWidget(m_dietGroup);
    l3->addSpacing(26);
    l3->addWidget(rule(p3));
    l3->addSpacing(25);
    l3->addWidget(m_preferenceGroup);
    l3->addStretch();

    auto *p4 = new QWidget(m_wizardStack);
    auto *l4 = new QVBoxLayout(p4);
    l4->setContentsMargins(0, 0, 0, 0);
    l4->setSpacing(0);
    auto *t4 = new QLabel(QStringLiteral("设定你的目标"), p4);
    t4->setObjectName(QStringLiteral("LoginFormTitle"));
    t4->setFont(UiAssets::titleFont(27));
    auto *c4 = copyLabel(QStringLiteral("选择一个主要目标，我们会据此计算建议热量。"), p4);
    m_goalCombo = new QComboBox(p4);
    m_goalCombo->addItem(QStringLiteral("保持健康"), QStringLiteral("maintain"));
    m_goalCombo->addItem(QStringLiteral("减脂"), QStringLiteral("lose"));
    m_goalCombo->addItem(QStringLiteral("增肌"), QStringLiteral("gain"));
    m_goalCombo->setCurrentIndex(-1);
    m_goalCombo->hide();
    m_goalButtons = new QButtonGroup(p4);
    m_goalButtons->setExclusive(true);
    auto *goalRow = new QHBoxLayout;
    goalRow->setSpacing(16);
    const QStringList goalTitles = {
        QStringLiteral("保持健康\n维持当前状态，获得\n均衡营养与长期健康"),
        QStringLiteral("减脂\n减少体脂，塑造线条，\n提升身体轻盈感"),
        QStringLiteral("增肌\n增加肌肉量，提升力量，\n改善身体表现")};
    const QStringList goalIcons = {QStringLiteral("goal-maintain"),
                                   QStringLiteral("goal-fat-loss"),
                                   QStringLiteral("goal-muscle-gain")};
    for (int i = 0; i < goalTitles.size(); ++i) {
        auto *button = new QToolButton(p4);
        button->setObjectName(QStringLiteral("RegisterGoalCard"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIcon(UiAssets::svgIcon(goalIcons.at(i), QColor(), QSize(62, 62), button));
        button->setIconSize(QSize(62, 62));
        button->setText(goalTitles.at(i));
        button->setFixedHeight(204);
        m_goalButtons->addButton(button, i);
        goalRow->addWidget(button, 1);
    }
    connect(m_goalButtons, &QButtonGroup::idClicked, this,
            [this](int id) { m_goalCombo->setCurrentIndex(id); });
    auto *targetGrid = new QGridLayout;
    targetGrid->setContentsMargins(0, 0, 0, 0);
    targetGrid->setHorizontalSpacing(41);
    targetGrid->setVerticalSpacing(5);
    m_targetWeightSpin = new QDoubleSpinBox(p4);
    configureSpinBox(m_targetWeightSpin);
    m_targetWeightSpin->setRange(30, 250);
    m_targetWeightSpin->setDecimals(0);
    m_targetWeightSpin->setValue(30);
    m_targetWeightSpin->setSpecialValueText(QStringLiteral("请输入目标体重"));
    UiAssets::attachFixedUnit(m_targetWeightSpin, QStringLiteral("kg"));
    m_planWeeksSpin = new QSpinBox(p4);
    configureSpinBox(m_planWeeksSpin);
    m_planWeeksSpin->setRange(1, 52);
    m_planWeeksSpin->setValue(1);
    m_planWeeksSpin->setSpecialValueText(QStringLiteral("请输入计划周期"));
    UiAssets::attachFixedUnit(m_planWeeksSpin, QStringLiteral("周"));
    targetGrid->addWidget(fieldLabel(QStringLiteral("目标体重"), p4, false), 0, 0);
    targetGrid->addWidget(fieldLabel(QStringLiteral("计划周期"), p4, false), 0, 1);
    targetGrid->addWidget(m_targetWeightSpin, 1, 0);
    targetGrid->addWidget(m_planWeeksSpin, 1, 1);
    auto *finishInfo = new QFrame(p4);
    finishInfo->setObjectName(QStringLiteral("RegisterFinishInfo"));
    auto *finishInfoLayout = new QHBoxLayout(finishInfo);
    finishInfoLayout->setContentsMargins(14, 10, 14, 10);
    finishInfoLayout->setSpacing(12);
    finishInfoLayout->addWidget(UiAssets::createIconLabel(
        finishInfo, QStringLiteral("preference-bowl"), 30, QColor(QStringLiteral("#08A96E"))));
    auto *finishCopy = new QVBoxLayout;
    finishCopy->setSpacing(2);
    auto *finishTitle = new QLabel(QStringLiteral("完成注册后即可生成首份个性化膳食方案"), finishInfo);
    finishTitle->setObjectName(QStringLiteral("RegisterFinishTitle"));
    auto *finishBody = new QLabel(
        QStringLiteral("我们会根据你的信息与目标，生成专属的每日营养建议与膳食计划。"), finishInfo);
    finishBody->setObjectName(QStringLiteral("RegisterFinishBody"));
    finishCopy->addWidget(finishTitle);
    finishCopy->addWidget(finishBody);
    finishInfoLayout->addLayout(finishCopy, 1);
    l4->addWidget(t4);
    l4->addSpacing(7);
    l4->addWidget(c4);
    l4->addSpacing(22);
    l4->addLayout(goalRow);
    l4->addSpacing(20);
    l4->addLayout(targetGrid);
    l4->addSpacing(16);
    l4->addWidget(finishInfo);
    l4->addStretch();

    for (QWidget *step : {p0, p1, p2, p3, p4})
        m_wizardStack->addWidget(step);

    m_hintLabel = new QLabel(regRoot);
    m_hintLabel->setObjectName(QStringLiteral("LoginHint"));
    m_hintLabel->setFixedHeight(18);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_navHost = new QWidget(regRoot);
    m_navHost->setObjectName(QStringLiteral("RegisterNavigation"));
    m_navHost->setFixedHeight(54);
    auto *navLayout = new QHBoxLayout(m_navHost);
    navLayout->setContentsMargins(0, 0, 0, 0);
    navLayout->setSpacing(18);
    m_backBtn = new QPushButton(QStringLiteral("上一步"), m_navHost);
    m_backBtn->setObjectName(QStringLiteral("RegBackButton"));
    m_nextBtn = new QPushButton(QStringLiteral("下一步"), m_navHost);
    m_nextBtn->setObjectName(QStringLiteral("RegNextButton"));
    for (QPushButton *button : {m_backBtn, m_nextBtn}) {
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedHeight(54);
        button->setFont(UiAssets::bodyFont(16, QFont::DemiBold));
        navLayout->addWidget(button, 1);
    }
    regLayout->addWidget(m_wizardStack, 1);
    regLayout->addWidget(m_hintLabel);
    regLayout->addSpacing(6);
    regLayout->addWidget(m_navHost);

    m_formStack->addWidget(loginPage);
    m_formStack->addWidget(regRoot);
    shellLayout->addWidget(railStack);
    shellLayout->addWidget(m_formStack, 1);
    page->addWidget(m_shell);

    connect(m_registerTab, &QPushButton::clicked, this, &LoginDialog::showRegister);
    connect(m_returnLoginBtn, &QPushButton::clicked, this, &LoginDialog::showLogin);
    connect(loginBtn, &QPushButton::clicked, this, &LoginDialog::onLogin);
    connect(m_loginPasswordEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLogin);
    connect(m_nextBtn, &QPushButton::clicked, this, &LoginDialog::onWizardNext);
    connect(m_backBtn, &QPushButton::clicked, this, &LoginDialog::onWizardBack);
    connect(forgot, &QPushButton::clicked, this, [this]() {
        setHint(QStringLiteral("本地账户暂不支持自动找回密码。"), false);
    });

    setMode(false);
    goRegisterStep(0);
    startupTrace("login_ctor_end");
}

User LoginDialog::user() const { return m_user; }

void LoginDialog::setReviewState(int page)
{
    if (page <= 0) {
        setMode(false);
        return;
    }
    setMode(true);
    goRegisterStep(qBound(0, page - 1, kRegSteps - 1));
    if (m_nextBtn)
        m_nextBtn->setFocus(Qt::OtherFocusReason);
}

void LoginDialog::setMode(bool registerMode)
{
    if (auto *railStack = m_shell->findChild<QStackedWidget *>(QStringLiteral("LoginRailStack")))
        railStack->setCurrentIndex(registerMode ? 1 : 0);
    m_formStack->setCurrentIndex(registerMode ? 1 : 0);
    setHint({});
    if (registerMode) {
        goRegisterStep(0);
        m_registerNameEdit->setFocus();
    } else {
        m_loginNameEdit->setFocus();
    }
}

void LoginDialog::showLogin() { setMode(false); }
void LoginDialog::showRegister() { setMode(true); }

void LoginDialog::setHint(const QString &text, bool error)
{
    if (m_hintLabel) {
        m_hintLabel->setText(text);
        m_hintLabel->setProperty("error", error);
        repolish(m_hintLabel);
    }
    if (m_formStack && m_formStack->currentIndex() == 0 && !text.isEmpty())
        m_loginPasswordEdit->setToolTip(text);
}

void LoginDialog::updateStepRail()
{
    for (int i = 0; i < m_stepRows.size(); ++i) {
        const QString state = i < m_regStep ? QStringLiteral("complete")
                                           : (i == m_regStep ? QStringLiteral("active")
                                                             : QStringLiteral("pending"));
        m_stepRows[i]->setProperty("state", state);
        m_stepNumbers[i]->setProperty("state", state);
        m_stepChecks[i]->setProperty("state", state);
        m_stepChecks[i]->setVisible(i < m_regStep);
        repolish(m_stepRows[i]);
        repolish(m_stepNumbers[i]);
        repolish(m_stepChecks[i]);
    }
}

void LoginDialog::updateWizardChrome()
{
    m_nextBtn->setText(m_regStep == kRegSteps - 1 ? QStringLiteral("完成注册")
                                                  : QStringLiteral("下一步"));
    m_backBtn->setVisible(m_regStep > 0);
    if (auto *layout = qobject_cast<QHBoxLayout *>(m_navHost->layout())) {
        layout->setStretch(0, m_regStep > 0 ? 2 : 0);
        layout->setStretch(1, m_regStep > 0 ? 3 : 1);
    }
    updateStepRail();
}

bool LoginDialog::eventFilter(QObject *watched, QEvent *event)
{
    return QDialog::eventFilter(watched, event);
}

void LoginDialog::goRegisterStep(int step)
{
    m_regStep = qBound(0, step, kRegSteps - 1);
    m_wizardStack->setCurrentIndex(m_regStep);
    updateWizardChrome();
}

bool LoginDialog::validateRegisterStep(int step)
{
    if (step == 0) {
        const QString name = m_registerNameEdit->text().trimmed();
        const QString password = m_registerPasswordEdit->text();
        const QString confirm = m_registerPasswordConfirmEdit->text();
        if (name.length() < 4 || name.length() > 20) {
            setHint(QStringLiteral("用户名需为 4-20 个字符。"), true);
            m_registerNameEdit->setFocus();
            return false;
        }
        if (password.length() < 8) {
            setHint(QStringLiteral("密码至少 8 位。"), true);
            m_registerPasswordEdit->setFocus();
            return false;
        }
        if (password != confirm) {
            setHint(QStringLiteral("两次输入的密码不一致。"), true);
            m_registerPasswordConfirmEdit->setFocus();
            return false;
        }
        if (UserDAO().findByName(name).id > 0) {
            setHint(QStringLiteral("用户名“%1”已存在，请直接登录。").arg(name), true);
            return false;
        }
    } else if (step == 1) {
        if (m_genderCombo->currentIndex() < 0 || !selectedBirthDate().isValid()
            || m_heightSpin->value() == m_heightSpin->minimum()
            || m_weightSpin->value() == m_weightSpin->minimum()
            || m_activityCombo->currentData().toString().isEmpty()) {
            setHint(QStringLiteral("请完整填写基础信息。"), true);
            return false;
        }
    } else if (step == 4 && m_goalCombo->currentIndex() < 0) {
        setHint(QStringLiteral("请选择一个主要目标。"), true);
        return false;
    }
    setHint({});
    return true;
}

QDate LoginDialog::selectedBirthDate() const
{
    if (!m_birthYearCombo || !m_birthMonthCombo || !m_birthDayCombo)
        return {};
    return QDate(m_birthYearCombo->currentData().toInt(),
                 m_birthMonthCombo->currentData().toInt(),
                 m_birthDayCombo->currentData().toInt());
}

void LoginDialog::updateBirthDays()
{
    if (!m_birthDayCombo)
        return;
    const int previousDay = m_birthDayCombo->currentData().toInt();
    const int year = m_birthYearCombo ? m_birthYearCombo->currentData().toInt() : 0;
    const int month = m_birthMonthCombo ? m_birthMonthCombo->currentData().toInt() : 0;
    int dayCount = 31;
    if (year > 0 && month > 0)
        dayCount = QDate(year, month, 1).daysInMonth();
    const QSignalBlocker blocker(m_birthDayCombo);
    m_birthDayCombo->clear();
    m_birthDayCombo->setPlaceholderText(QStringLiteral("日期"));
    for (int day = 1; day <= dayCount; ++day)
        m_birthDayCombo->addItem(QStringLiteral("%1日").arg(day), day);
    const int restoredIndex = m_birthDayCombo->findData(previousDay);
    m_birthDayCombo->setCurrentIndex(restoredIndex);
}

void LoginDialog::onWizardNext()
{
    if (!validateRegisterStep(m_regStep))
        return;
    if (m_regStep >= kRegSteps - 1) {
        onRegisterFinish();
        return;
    }
    goRegisterStep(m_regStep + 1);
}

void LoginDialog::onWizardBack()
{
    if (m_regStep > 0)
        goRegisterStep(m_regStep - 1);
}

void LoginDialog::onLogin()
{
    startupTrace("login_click");
    const QString name = m_loginNameEdit->text().trimmed();
    const QString password = m_loginPasswordEdit->text();
    if (name.isEmpty()) {
        setHint(QStringLiteral("请输入用户名。"), true);
        return;
    }
    if (password.isEmpty()) {
        setHint(QStringLiteral("请输入密码。"), true);
        return;
    }
    const User existing = UserDAO().findByName(name);
    if (existing.id <= 0) {
        setHint(QStringLiteral("没有找到“%1”，请先注册。").arg(name), true);
        return;
    }
    if (existing.passwordHash.isEmpty()) {
        setHint(QStringLiteral("该账号尚未设置密码，请重新注册。"), true);
        return;
    }
    const User authenticated = UserDAO().authenticate(name, password);
    if (authenticated.id <= 0) {
        setHint(QStringLiteral("密码不正确，请重试。"), true);
        return;
    }
    m_user = authenticated;
    startupTrace("login_accept");
    QTimer::singleShot(0, this, [this]() { accept(); });
}

void LoginDialog::onRegisterFinish()
{
    startupTrace("register_click");
    if (!DatabaseManager::getInstance().isOpen()) {
        setHint(QStringLiteral("数据库未连接，无法注册。"), true);
        return;
    }
    if (!validateRegisterStep(0) || !validateRegisterStep(1) || !validateRegisterStep(4))
        return;

    User user;
    user.name = m_registerNameEdit->text().trimmed();
    user.gender = m_genderCombo->currentData().toString();
    user.goal = m_goalCombo->currentData().toString();
    user.height = m_heightSpin->value();
    user.weight = m_weightSpin->value();
    const QDate birth = selectedBirthDate();
    const QDate today = QDate::currentDate();
    user.age = today.year() - birth.year()
               - (today < QDate(today.year(), birth.month(), birth.day()) ? 1 : 0);
    user.passwordHash = AuthUtils::hashPassword(m_registerPasswordEdit->text());
    user.preferences = m_preferenceGroup ? m_preferenceGroup->selected().join(QStringLiteral("、"))
                                         : QString();
    user.dietaryChoices = m_dietGroup ? m_dietGroup->selected() : QStringList{};
    user.allergies = m_allergyGroup ? m_allergyGroup->selected() : QStringList{};
    user.foodIntolerances = m_intoleranceGroup ? m_intoleranceGroup->selected() : QStringList{};
    user.nutritionalDeficiencies = {};
    user.medicalConditions = m_medicalGroup ? m_medicalGroup->selected() : QStringList{};
    auto removeNone = [](QStringList &values) {
        values.erase(std::remove_if(values.begin(), values.end(), [](const QString &value) {
                         return value.contains(QStringLiteral("无相关情况"))
                                || value.contains(QStringLiteral("暂无"));
                     }), values.end());
    };
    removeNone(user.allergies);
    removeNone(user.foodIntolerances);
    removeNone(user.medicalConditions);
    user.syncAllergenFields();
    user.calorieTarget = UserService().calculateDailyCalories(user);

    UserDAO dao;
    if (!dao.insertUser(user)) {
        setHint(QStringLiteral("创建失败：数据库写入异常。"), true);
        return;
    }
    m_user = dao.authenticate(user.name, m_registerPasswordEdit->text());
    if (m_user.id <= 0) {
        QMessageBox::warning(this, QStringLiteral("创建失败"),
                             QStringLiteral("用户已保存，但自动登录失败。"));
        return;
    }
    startupTrace("register_accept");
    QTimer::singleShot(0, this, [this]() { accept(); });
}
