#include "LoginDialog.h"
#include "LogoWidget.h"
#include "TagChipGroup.h"

#include "../dao/DatabaseManager.h"
#include "../dao/UserDAO.h"
#include "../services/AuthUtils.h"
#include "../services/UserService.h"

#include <QAbstractSpinBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSlider>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

namespace {
void startupTrace(const char *step)
{
    const QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/startup.log");
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        f.write(step);
        f.write("\n");
        f.flush();
    }
}

QLabel *makeFieldLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setProperty("class", QVariant(QStringLiteral("LoginFieldLabel")));
    return label;
}

QFrame *makeRule(QWidget *parent)
{
    auto *rule = new QFrame(parent);
    rule->setFrameShape(QFrame::HLine);
    rule->setObjectName(QStringLiteral("LoginRule"));
    return rule;
}

QLineEdit *makePasswordEdit(QWidget *parent, const QString &placeholder)
{
    auto *edit = new QLineEdit(parent);
    edit->setObjectName(QStringLiteral("LoginInput"));
    edit->setEchoMode(QLineEdit::Password);
    edit->setPlaceholderText(placeholder);
    edit->setClearButtonEnabled(true);
    return edit;
}

QWidget *wrapScroll(QWidget *inner)
{
    inner->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setWidget(inner);
    scroll->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    return scroll;
}
} // namespace

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("膳衡 · 欢迎使用"));
    setModal(true);
    setFixedSize(920, 720);
    setObjectName(QStringLiteral("LoginDialog"));

    auto *page = new QVBoxLayout(this);
    page->setContentsMargins(0, 0, 0, 0);
    page->setAlignment(Qt::AlignCenter);

    auto *shell = new QFrame(this);
    shell->setObjectName(QStringLiteral("LoginShell"));
    shell->setFixedSize(780, 600);
    auto *shellLayout = new QHBoxLayout(shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    auto *story = new QFrame(shell);
    story->setObjectName(QStringLiteral("LoginStory"));
    story->setFixedWidth(290);
    auto *storyLayout = new QVBoxLayout(story);
    storyLayout->setContentsMargins(34, 42, 32, 36);
    auto *logo = new LogoWidget(story);
    logo->setFixedSize(54, 54);
    auto *brand = new QLabel(QStringLiteral("膳衡"), story);
    brand->setObjectName(QStringLiteral("LoginBrand"));
    auto *brandEn = new QLabel(QStringLiteral("SMART DIET"), story);
    brandEn->setObjectName(QStringLiteral("LoginBrandEn"));
    auto *storyTitle = new QLabel(QStringLiteral("让每一餐，\n更靠近你的目标。"), story);
    storyTitle->setObjectName(QStringLiteral("LoginStoryTitle"));
    storyTitle->setWordWrap(true);
    auto *storyCopy = new QLabel(QStringLiteral("分步建立多维健康档案\n本地营养库 · RDSS 推荐。"), story);
    storyCopy->setObjectName(QStringLiteral("LoginStoryCopy"));
    storyCopy->setWordWrap(true);
    auto *privacy = new QLabel(QStringLiteral("●  本地模式 · 数据安全存储于此设备"), story);
    privacy->setObjectName(QStringLiteral("LoginPrivacy"));
    storyLayout->addWidget(logo, 0, Qt::AlignLeft);
    storyLayout->addSpacing(14);
    storyLayout->addWidget(brand);
    storyLayout->addWidget(brandEn);
    storyLayout->addStretch(1);
    storyLayout->addWidget(storyTitle);
    storyLayout->addSpacing(14);
    storyLayout->addWidget(storyCopy);
    storyLayout->addStretch(2);
    storyLayout->addWidget(privacy);

    auto *formArea = new QFrame(shell);
    formArea->setObjectName(QStringLiteral("LoginFormArea"));
    auto *formLayout = new QVBoxLayout(formArea);
    formLayout->setContentsMargins(42, 22, 42, 18);
    formLayout->setSpacing(0);

    auto *tabs = new QHBoxLayout;
    tabs->setSpacing(24);
    m_loginTab = new QPushButton(QStringLiteral("登录"), formArea);
    m_registerTab = new QPushButton(QStringLiteral("注册"), formArea);
    for (QPushButton *tab : {m_loginTab, m_registerTab}) {
        tab->setCheckable(true);
        tab->setCursor(Qt::PointingHandCursor);
        tab->setProperty("class", QVariant(QStringLiteral("LoginTab")));
        tabs->addWidget(tab);
    }
    tabs->addStretch();

    m_formStack = new QStackedWidget(formArea);

    // ---- Login ----
    auto *loginPage = new QWidget(m_formStack);
    auto *loginLayout = new QVBoxLayout(loginPage);
    loginLayout->setContentsMargins(0, 0, 0, 0);
    auto *loginTitle = new QLabel(QStringLiteral("欢迎回来"), loginPage);
    loginTitle->setObjectName(QStringLiteral("LoginFormTitle"));
    auto *loginCopy = new QLabel(QStringLiteral("使用用户名与密码登录本地膳食档案。"), loginPage);
    loginCopy->setProperty("class", QVariant(QStringLiteral("LoginFormCopy")));
    m_loginNameEdit = new QLineEdit(loginPage);
    m_loginNameEdit->setObjectName(QStringLiteral("LoginInput"));
    m_loginNameEdit->setPlaceholderText(QStringLiteral("用户名，例如：张明"));
    m_loginNameEdit->setClearButtonEnabled(true);
    m_loginPasswordEdit = makePasswordEdit(loginPage, QStringLiteral("请输入密码"));
    auto *loginBtn = new QPushButton(QStringLiteral("登录膳衡  →"), loginPage);
    loginBtn->setObjectName(QStringLiteral("LoginPrimaryAction"));
    loginBtn->setCursor(Qt::PointingHandCursor);
    auto *loginTip = new QLabel(QStringLiteral("演示账号：张明 / 123456。首次使用可点击上方“注册”。"), loginPage);
    loginTip->setProperty("class", QVariant(QStringLiteral("LoginTip")));
    loginTip->setWordWrap(true);
    loginLayout->addSpacing(28);
    loginLayout->addWidget(loginTitle);
    loginLayout->addSpacing(8);
    loginLayout->addWidget(loginCopy);
    loginLayout->addSpacing(22);
    loginLayout->addWidget(makeFieldLabel(QStringLiteral("用户名"), loginPage));
    loginLayout->addSpacing(6);
    loginLayout->addWidget(m_loginNameEdit);
    loginLayout->addSpacing(14);
    loginLayout->addWidget(makeFieldLabel(QStringLiteral("密码"), loginPage));
    loginLayout->addSpacing(6);
    loginLayout->addWidget(m_loginPasswordEdit);
    loginLayout->addSpacing(22);
    loginLayout->addWidget(loginBtn);
    loginLayout->addSpacing(16);
    loginLayout->addWidget(loginTip);
    loginLayout->addStretch();

    // ---- Register wizard ----
    auto *regRoot = new QWidget(m_formStack);
    auto *regLay = new QVBoxLayout(regRoot);
    regLay->setContentsMargins(0, 0, 0, 0);
    regLay->setSpacing(8);

    m_stepLabel = new QLabel(regRoot);
    m_stepLabel->setObjectName(QStringLiteral("LoginStepLabel"));

    m_wizardStack = new QStackedWidget(regRoot);

    // Step 0: account
    auto *p0 = new QWidget;
    auto *l0 = new QVBoxLayout(p0);
    l0->setContentsMargins(0, 0, 8, 0);
    auto *t0 = new QLabel(QStringLiteral("创建账号"), p0);
    t0->setObjectName(QStringLiteral("LoginFormTitle"));
    auto *c0 = new QLabel(QStringLiteral("第 1 步 · 设置用户名与密码"), p0);
    c0->setProperty("class", QVariant(QStringLiteral("LoginFormCopy")));
    m_registerNameEdit = new QLineEdit(p0);
    m_registerNameEdit->setObjectName(QStringLiteral("LoginInput"));
    m_registerNameEdit->setPlaceholderText(QStringLiteral("请输入用户名"));
    m_registerPasswordEdit = makePasswordEdit(p0, QStringLiteral("至少 6 位密码"));
    m_registerPasswordConfirmEdit = makePasswordEdit(p0, QStringLiteral("再次输入密码"));
    l0->addWidget(t0);
    l0->addWidget(c0);
    l0->addSpacing(12);
    l0->addWidget(makeFieldLabel(QStringLiteral("用户名"), p0));
    l0->addWidget(m_registerNameEdit);
    l0->addSpacing(8);
    l0->addWidget(makeFieldLabel(QStringLiteral("密码"), p0));
    l0->addWidget(m_registerPasswordEdit);
    l0->addSpacing(8);
    l0->addWidget(makeFieldLabel(QStringLiteral("确认密码"), p0));
    l0->addWidget(m_registerPasswordConfirmEdit);
    l0->addStretch();

    // Step 1: body
    auto *p1 = new QWidget;
    auto *l1 = new QVBoxLayout(p1);
    l1->setContentsMargins(0, 0, 8, 0);
    auto *t1 = new QLabel(QStringLiteral("身体数据"), p1);
    t1->setObjectName(QStringLiteral("LoginFormTitle"));
    auto *c1 = new QLabel(QStringLiteral("第 2 步 · 身高、体重与性别"), p1);
    c1->setProperty("class", QVariant(QStringLiteral("LoginFormCopy")));
    m_genderCombo = new QComboBox(p1);
    m_genderCombo->setObjectName(QStringLiteral("LoginCombo"));
    m_genderCombo->addItem(QStringLiteral("男"), QStringLiteral("male"));
    m_genderCombo->addItem(QStringLiteral("女"), QStringLiteral("female"));
    auto *heightRow = new QHBoxLayout;
    m_heightSlider = new QSlider(Qt::Horizontal, p1);
    m_heightSlider->setObjectName(QStringLiteral("ProfileSlider"));
    m_heightSlider->setRange(140, 210);
    m_heightSlider->setValue(170);
    m_heightSpin = new QDoubleSpinBox(p1);
    m_heightSpin->setObjectName(QStringLiteral("LoginNumber"));
    m_heightSpin->setRange(140, 210);
    m_heightSpin->setDecimals(0);
    m_heightSpin->setValue(170);
    m_heightSpin->setFixedWidth(72);
    m_heightSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    heightRow->addWidget(m_heightSlider, 1);
    heightRow->addWidget(m_heightSpin);
    heightRow->addWidget(new QLabel(QStringLiteral("cm"), p1));
    auto *weightRow = new QHBoxLayout;
    m_weightSlider = new QSlider(Qt::Horizontal, p1);
    m_weightSlider->setObjectName(QStringLiteral("ProfileSlider"));
    m_weightSlider->setRange(35, 150);
    m_weightSlider->setValue(65);
    m_weightSpin = new QDoubleSpinBox(p1);
    m_weightSpin->setObjectName(QStringLiteral("LoginNumber"));
    m_weightSpin->setRange(35, 150);
    m_weightSpin->setDecimals(0);
    m_weightSpin->setValue(65);
    m_weightSpin->setFixedWidth(72);
    m_weightSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    weightRow->addWidget(m_weightSlider, 1);
    weightRow->addWidget(m_weightSpin);
    weightRow->addWidget(new QLabel(QStringLiteral("kg"), p1));
    l1->addWidget(t1);
    l1->addWidget(c1);
    l1->addSpacing(12);
    l1->addWidget(makeFieldLabel(QStringLiteral("性别"), p1));
    l1->addWidget(m_genderCombo);
    l1->addSpacing(8);
    l1->addWidget(makeFieldLabel(QStringLiteral("身高"), p1));
    l1->addLayout(heightRow);
    l1->addSpacing(8);
    l1->addWidget(makeFieldLabel(QStringLiteral("体重"), p1));
    l1->addLayout(weightRow);
    l1->addStretch();

    // Step 2: diet + allergy
    auto *p2inner = new QWidget;
    auto *l2 = new QVBoxLayout(p2inner);
    l2->setContentsMargins(0, 0, 8, 0);
    auto *t2 = new QLabel(QStringLiteral("饮食与过敏"), p2inner);
    t2->setObjectName(QStringLiteral("LoginFormTitle"));
    auto *c2 = new QLabel(QStringLiteral("第 3 步 · 饮食选择与过敏史（可多选，可跳过）"), p2inner);
    c2->setProperty("class", QVariant(QStringLiteral("LoginFormCopy")));
    c2->setWordWrap(true);
    m_dietGroup = new TagChipGroup(
        QStringLiteral("饮食选择"),
        {QStringLiteral("荤食者"), QStringLiteral("素食者"), QStringLiteral("蛋奶素食者"),
         QStringLiteral("严格素食者"), QStringLiteral("清真"), QStringLiteral("避免红肉")},
        p2inner);
    m_allergyGroup = new TagChipGroup(
        QStringLiteral("过敏史"),
        {QStringLiteral("花生"), QStringLiteral("坚果"), QStringLiteral("鸡蛋"), QStringLiteral("牛奶"),
         QStringLiteral("海鲜"), QStringLiteral("贝类"), QStringLiteral("大豆"), QStringLiteral("豆制品"),
         QStringLiteral("麸质（小麦）"), QStringLiteral("芝麻"), QStringLiteral("猕猴桃")},
        p2inner);
    l2->addWidget(t2);
    l2->addWidget(c2);
    l2->addSpacing(8);
    l2->addWidget(m_dietGroup);
    l2->addWidget(m_allergyGroup);

    // Step 3: intolerance + deficiency + medical
    auto *p3inner = new QWidget;
    auto *l3 = new QVBoxLayout(p3inner);
    l3->setContentsMargins(0, 0, 8, 0);
    auto *t3 = new QLabel(QStringLiteral("不耐受与营养"), p3inner);
    t3->setObjectName(QStringLiteral("LoginFormTitle"));
    auto *c3 = new QLabel(QStringLiteral("第 4 步 · 不耐受、营养缺乏与医疗状况（可跳过）"), p3inner);
    c3->setProperty("class", QVariant(QStringLiteral("LoginFormCopy")));
    c3->setWordWrap(true);
    m_intoleranceGroup = new TagChipGroup(
        QStringLiteral("食物不耐受"),
        {QStringLiteral("乳糖"), QStringLiteral("麸质"), QStringLiteral("果糖"),
         QStringLiteral("水杨酸盐"), QStringLiteral("亚硫酸盐"), QStringLiteral("FODMAPs")},
        p3inner);
    m_deficiencyGroup = new TagChipGroup(
        QStringLiteral("营养缺乏"),
        {QStringLiteral("缺铁"), QStringLiteral("缺钙"), QStringLiteral("缺维生素D"),
         QStringLiteral("缺维生素B12"), QStringLiteral("蛋白质不足")},
        p3inner);
    m_medicalGroup = new TagChipGroup(
        QStringLiteral("医疗状况"),
        {QStringLiteral("2型糖尿病"), QStringLiteral("高血压"), QStringLiteral("高血脂"),
         QStringLiteral("心血管疾病"), QStringLiteral("肥胖"), QStringLiteral("贫血"),
         QStringLiteral("肾病")},
        p3inner);
    l3->addWidget(t3);
    l3->addWidget(c3);
    l3->addSpacing(8);
    l3->addWidget(m_intoleranceGroup);
    l3->addWidget(m_deficiencyGroup);
    l3->addWidget(m_medicalGroup);

    // Step 4: goal
    auto *p4 = new QWidget;
    auto *l4 = new QVBoxLayout(p4);
    l4->setContentsMargins(0, 0, 8, 0);
    auto *t4 = new QLabel(QStringLiteral("健康目标"), p4);
    t4->setObjectName(QStringLiteral("LoginFormTitle"));
    auto *c4 = new QLabel(QStringLiteral("第 5 步 · 选择减重 / 增肌 / 维持"), p4);
    c4->setProperty("class", QVariant(QStringLiteral("LoginFormCopy")));
    m_goalCombo = new QComboBox(p4);
    m_goalCombo->setObjectName(QStringLiteral("LoginCombo"));
    m_goalCombo->addItem(QStringLiteral("减重 · 控制热量"), QStringLiteral("lose"));
    m_goalCombo->addItem(QStringLiteral("增肌 · 蛋白质优先"), QStringLiteral("gain"));
    m_goalCombo->addItem(QStringLiteral("维持 · 均衡饮食"), QStringLiteral("maintain"));
    m_goalCombo->setCurrentIndex(2);
    l4->addWidget(t4);
    l4->addWidget(c4);
    l4->addSpacing(16);
    l4->addWidget(makeFieldLabel(QStringLiteral("目标"), p4));
    l4->addWidget(m_goalCombo);
    l4->addStretch();

    m_wizardStack->addWidget(p0);
    m_wizardStack->addWidget(p1);
    m_wizardStack->addWidget(wrapScroll(p2inner));
    m_wizardStack->addWidget(wrapScroll(p3inner));
    m_wizardStack->addWidget(p4);

    auto *nav = new QHBoxLayout;
    m_backBtn = new QPushButton(QStringLiteral("上一步"), regRoot);
    m_backBtn->setProperty("class", QVariant(QStringLiteral("GhostButton")));
    m_backBtn->setCursor(Qt::PointingHandCursor);
    m_nextBtn = new QPushButton(QStringLiteral("下一步"), regRoot);
    m_nextBtn->setObjectName(QStringLiteral("LoginPrimaryAction"));
    m_nextBtn->setCursor(Qt::PointingHandCursor);
    nav->addWidget(m_backBtn);
    nav->addStretch();
    nav->addWidget(m_nextBtn);

    regLay->addWidget(m_stepLabel);
    regLay->addWidget(m_wizardStack, 1);
    regLay->addLayout(nav);

    m_formStack->addWidget(loginPage);
    m_formStack->addWidget(regRoot);

    m_hintLabel = new QLabel(formArea);
    m_hintLabel->setObjectName(QStringLiteral("LoginHint"));
    m_hintLabel->setMinimumHeight(20);
    m_hintLabel->setWordWrap(true);

    formLayout->addLayout(tabs);
    formLayout->addWidget(makeRule(formArea));
    formLayout->addWidget(m_formStack, 1);
    formLayout->addWidget(m_hintLabel);

    shellLayout->addWidget(story);
    shellLayout->addWidget(formArea, 1);
    page->addWidget(shell);

    connect(m_loginTab, &QPushButton::clicked, this, &LoginDialog::showLogin);
    connect(m_registerTab, &QPushButton::clicked, this, &LoginDialog::showRegister);
    connect(loginBtn, &QPushButton::clicked, this, &LoginDialog::onLogin);
    connect(m_loginPasswordEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLogin);
    connect(m_nextBtn, &QPushButton::clicked, this, &LoginDialog::onWizardNext);
    connect(m_backBtn, &QPushButton::clicked, this, &LoginDialog::onWizardBack);
    connect(m_heightSlider, &QSlider::valueChanged, this, &LoginDialog::syncHeightFromSlider);
    connect(m_heightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &LoginDialog::syncHeightFromSpin);
    connect(m_weightSlider, &QSlider::valueChanged, this, &LoginDialog::syncWeightFromSlider);
    connect(m_weightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &LoginDialog::syncWeightFromSpin);

    setMode(false);
    goRegisterStep(0);
}

User LoginDialog::user() const
{
    return m_user;
}

void LoginDialog::setMode(bool registerMode)
{
    m_formStack->setCurrentIndex(registerMode ? 1 : 0);
    m_loginTab->setChecked(!registerMode);
    m_registerTab->setChecked(registerMode);
    setHint({});
    if (registerMode) {
        goRegisterStep(0);
        m_registerNameEdit->setFocus();
    } else {
        m_loginNameEdit->setFocus();
    }
}

void LoginDialog::showLogin()
{
    setMode(false);
}

void LoginDialog::showRegister()
{
    setMode(true);
}

void LoginDialog::setHint(const QString &text, bool error)
{
    m_hintLabel->setText(text);
    m_hintLabel->setProperty("error", error);
    m_hintLabel->style()->unpolish(m_hintLabel);
    m_hintLabel->style()->polish(m_hintLabel);
}

void LoginDialog::updateWizardChrome()
{
    static const char *titles[] = {"账号密码", "身体数据", "饮食过敏", "营养医疗", "健康目标"};
    m_stepLabel->setText(QStringLiteral("注册进度  %1 / %2  ·  %3")
                             .arg(m_regStep + 1)
                             .arg(kRegSteps)
                             .arg(QString::fromUtf8(titles[m_regStep])));
    m_backBtn->setEnabled(m_regStep > 0);
    m_nextBtn->setText(m_regStep >= kRegSteps - 1 ? QStringLiteral("完成注册  →")
                                                 : QStringLiteral("下一步"));
}

void LoginDialog::goRegisterStep(int step)
{
    step = qBound(0, step, kRegSteps - 1);
    m_regStep = step;
    m_wizardStack->setCurrentIndex(step);
    updateWizardChrome();
}

bool LoginDialog::validateRegisterStep(int step)
{
    if (step == 0) {
        const QString name = m_registerNameEdit->text().trimmed();
        const QString password = m_registerPasswordEdit->text();
        const QString confirm = m_registerPasswordConfirmEdit->text();
        if (name.isEmpty()) {
            setHint(QStringLiteral("请先填写用户名。"), true);
            m_registerNameEdit->setFocus();
            return false;
        }
        if (password.length() < 6) {
            setHint(QStringLiteral("密码至少 6 位。"), true);
            m_registerPasswordEdit->setFocus();
            return false;
        }
        if (password != confirm) {
            setHint(QStringLiteral("两次输入的密码不一致。"), true);
            m_registerPasswordConfirmEdit->setFocus();
            return false;
        }
        UserDAO dao;
        if (dao.findByName(name).id > 0) {
            setHint(QStringLiteral("用户名“%1”已存在，请直接登录。").arg(name), true);
            return false;
        }
    }
    setHint({});
    return true;
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
    if (m_regStep <= 0)
        return;
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

    UserDAO dao;
    const User existing = dao.findByName(name);
    if (existing.id <= 0) {
        setHint(QStringLiteral("没有找到“%1”，请先注册。").arg(name), true);
        return;
    }
    if (existing.passwordHash.isEmpty()) {
        setHint(QStringLiteral("该账号尚未设置密码，请重新注册。"), true);
        return;
    }
    const User authed = dao.authenticate(name, password);
    if (authed.id <= 0) {
        setHint(QStringLiteral("密码不正确，请重试。"), true);
        return;
    }
    m_user = authed;
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
    if (!validateRegisterStep(0))
        return;

    User newUser;
    newUser.name = m_registerNameEdit->text().trimmed();
    newUser.gender = m_genderCombo->currentData().toString();
    newUser.goal = m_goalCombo->currentData().toString();
    newUser.height = m_heightSpin->value();
    newUser.weight = m_weightSpin->value();
    newUser.passwordHash = AuthUtils::hashPassword(m_registerPasswordEdit->text());
    newUser.dietaryChoices = m_dietGroup->selected();
    newUser.allergies = m_allergyGroup->selected();
    newUser.foodIntolerances = m_intoleranceGroup->selected();
    newUser.nutritionalDeficiencies = m_deficiencyGroup->selected();
    newUser.medicalConditions = m_medicalGroup->selected();
    newUser.syncAllergenFields();

    UserService service;
    newUser.calorieTarget = service.calculateDailyCalories(newUser);

    UserDAO dao;
    if (!dao.insertUser(newUser)) {
        setHint(QStringLiteral("创建失败：数据库写入异常。"), true);
        return;
    }

    m_user = dao.authenticate(newUser.name, m_registerPasswordEdit->text());
    if (m_user.id <= 0) {
        QMessageBox::warning(this, QStringLiteral("创建失败"), QStringLiteral("用户已保存，但自动登录失败。"));
        return;
    }
    startupTrace("register_accept");
    QTimer::singleShot(0, this, [this]() { accept(); });
}

void LoginDialog::syncHeightFromSlider(int value)
{
    if (qRound(m_heightSpin->value()) != value)
        m_heightSpin->setValue(value);
}

void LoginDialog::syncHeightFromSpin(double value)
{
    const int rounded = qRound(value);
    if (m_heightSlider->value() != rounded)
        m_heightSlider->setValue(rounded);
}

void LoginDialog::syncWeightFromSlider(int value)
{
    if (qRound(m_weightSpin->value()) != value)
        m_weightSpin->setValue(value);
}

void LoginDialog::syncWeightFromSpin(double value)
{
    const int rounded = qRound(value);
    if (m_weightSlider->value() != rounded)
        m_weightSlider->setValue(rounded);
}
