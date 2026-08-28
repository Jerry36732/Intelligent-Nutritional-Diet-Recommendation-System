#include "MainWindow.h"

#include "DashboardWidget.h"
#include "FoodSearchWidget.h"
#include "LoginDialog.h"
#include "LogoWidget.h"
#include "ProfileWidget.h"
#include "RecipeDetailDialog.h"
#include "RecommendWidget.h"
#include "SettingsDialog.h"

#include "../dao/DatabaseManager.h"
#include "../dao/RecipeDAO.h"
#include "../dao/UserDAO.h"
#include "../engine/RecommendEngine.h"
#include "../services/AiAssistantService.h"
#include "../services/UserService.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("膳衡 · 智能营养膳食推荐系统"));
    resize(1180, 760);
    setMinimumSize(980, 680);

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("CentralRoot"));
    setCentralWidget(central);

    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- Sidebar ----
    auto *sidebar = new QFrame(central);
    sidebar->setObjectName(QStringLiteral("Sidebar"));
    auto *sideLay = new QVBoxLayout(sidebar);
    sideLay->setContentsMargins(18, 22, 18, 18);
    sideLay->setSpacing(10);

    auto *brandRow = new QWidget(sidebar);
    brandRow->setObjectName(QStringLiteral("BrandRow"));
    brandRow->setCursor(Qt::PointingHandCursor);
    brandRow->setToolTip(QStringLiteral("点击切换 AI 助手面板显示/隐藏"));
    m_brandRow = brandRow;
    auto *brandRowLayout = new QHBoxLayout(brandRow);
    brandRowLayout->setContentsMargins(0, 0, 0, 0);
    brandRowLayout->setSpacing(10);
    auto *brandLogo = new LogoWidget(brandRow);
    auto *brandText = new QVBoxLayout;
    brandText->setSpacing(1);
    auto *brand = new QLabel(QStringLiteral("膳衡"), brandRow);
    brand->setObjectName(QStringLiteral("BrandMark"));
    auto *brandSub = new QLabel(QStringLiteral("SMART DIET"), brandRow);
    brandSub->setObjectName(QStringLiteral("BrandSub"));
    brandText->addWidget(brand);
    brandText->addWidget(brandSub);
    brandRowLayout->addWidget(brandLogo);
    brandRowLayout->addLayout(brandText);
    brandRowLayout->addStretch();

    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);

    auto makeNav = [&](const QString &text, const QString &badge, int id) {
        // 避免给 QPushButton 嵌套 Layout（部分环境下易引发异常）
        auto *btn = new QPushButton(QStringLiteral("%1    %2").arg(text, badge), sidebar);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("class", QVariant(QStringLiteral("NavButton")));
        btn->setMinimumHeight(42);
        m_navGroup->addButton(btn, id);
        m_navButtons.append(btn);
        sideLay->addWidget(btn);
        return btn;
    };

    sideLay->addWidget(brandRow);
    auto *workspaceLabel = new QLabel(QStringLiteral("工作台"), sidebar);
    workspaceLabel->setObjectName(QStringLiteral("WorkspaceLabel"));
    sideLay->addSpacing(16);
    sideLay->addWidget(workspaceLabel);
    makeNav(QStringLiteral("今日方案"), QStringLiteral("01"), 0);
    makeNav(QStringLiteral("食材库"), QStringLiteral("100+"), 1);
    makeNav(QStringLiteral("智能推荐"), QStringLiteral("AI"), 2);
    makeNav(QStringLiteral("我的档案"), QStringLiteral("ME"), 3);
    sideLay->addStretch();

    auto *offline = new QFrame(sidebar);
    offline->setObjectName(QStringLiteral("OfflineCard"));
    auto *offLay = new QVBoxLayout(offline);
    offLay->setContentsMargins(12, 10, 12, 10);
    auto *offTitle = new QLabel(QStringLiteral("本地离线可用"), offline);
    offTitle->setObjectName(QStringLiteral("OfflineTitle"));
    auto *offBody = new QLabel(QStringLiteral("营养数据与推荐均在本地 SQLite 完成，无需联网。"), offline);
    offBody->setObjectName(QStringLiteral("OfflineBody"));
    offBody->setWordWrap(true);
    offLay->addWidget(offTitle);
    offLay->addWidget(offBody);

    auto *profileMini = new QFrame(sidebar);
    profileMini->setObjectName(QStringLiteral("ProfileMini"));
    profileMini->setCursor(Qt::PointingHandCursor);
    profileMini->setToolTip(QStringLiteral("点击打开用户设置"));
    m_profileMini = profileMini;
    auto *pmLay = new QVBoxLayout(profileMini);
    pmLay->setContentsMargins(12, 10, 12, 10);
    m_profileName = new QLabel(QStringLiteral("未登录"), profileMini);
    m_profileName->setObjectName(QStringLiteral("ProfileMiniName"));
    m_profileMeta = new QLabel(QStringLiteral("点击打开用户设置"), profileMini);
    m_profileMeta->setObjectName(QStringLiteral("ProfileMiniMeta"));
    m_profileMeta->setWordWrap(true);
    pmLay->addWidget(m_profileName);
    pmLay->addWidget(m_profileMeta);

    sideLay->addWidget(offline);
    sideLay->addWidget(profileMini);

    brandRow->installEventFilter(this);
    profileMini->installEventFilter(this);

    // ---- Right column ----
    auto *right = new QWidget(central);
    auto *rightLay = new QVBoxLayout(right);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(0);

    auto *topBar = new QFrame(right);
    topBar->setObjectName(QStringLiteral("TopBar"));
    auto *topLay = new QHBoxLayout(topBar);
    topLay->setContentsMargins(24, 14, 24, 14);

    auto *titleBox = new QVBoxLayout;
    titleBox->setSpacing(2);
    m_topDate = new QLabel(QDate::currentDate().toString(QStringLiteral("yyyy年M月d日 dddd")), topBar);
    m_topDate->setObjectName(QStringLiteral("TopDate"));
    m_topTitle = new QLabel(QStringLiteral("今日膳食方案"), topBar);
    m_topTitle->setObjectName(QStringLiteral("TopTitle"));
    titleBox->addWidget(m_topDate);
    titleBox->addWidget(m_topTitle);

    m_dbBadge = new QLabel(QStringLiteral("数据库未连接"), topBar);
    m_dbBadge->setObjectName(QStringLiteral("DbBadge"));
    m_dbBadge->setProperty("connected", false);

    auto *switchBtn = new QPushButton(QStringLiteral("切换用户"), topBar);
    switchBtn->setObjectName(QStringLiteral("SwitchUserBtn"));
    switchBtn->setCursor(Qt::PointingHandCursor);

    topLay->addLayout(titleBox);
    topLay->addStretch();
    topLay->addWidget(m_dbBadge);
    topLay->addWidget(switchBtn);

    m_stack = new QStackedWidget(right);
    m_dashboard = new DashboardWidget(m_stack);
    m_foods = new FoodSearchWidget(m_stack);
    m_recommend = new RecommendWidget(m_stack);
    m_profile = new ProfileWidget(m_stack);
    m_stack->addWidget(m_dashboard); // 0
    m_stack->addWidget(m_foods);     // 1
    m_stack->addWidget(m_recommend); // 2
    m_stack->addWidget(m_profile);   // 3

    auto *footer = new QLabel(
        QStringLiteral("免责声明：膳衡提供的膳食建议仅供参考，不能替代医师或注册营养师的专业意见。"),
        right);
    footer->setObjectName(QStringLiteral("FooterDisclaimer"));
    footer->setWordWrap(true);
    footer->setAlignment(Qt::AlignCenter);

    rightLay->addWidget(topBar);
    rightLay->addWidget(m_stack, 1);
    rightLay->addWidget(footer);

    root->addWidget(sidebar);
    root->addWidget(right, 1);

    connect(m_navGroup, &QButtonGroup::idClicked, this, &MainWindow::onNavChanged);
    connect(switchBtn, &QPushButton::clicked, this, &MainWindow::onSwitchUser);

    connect(m_dashboard, &DashboardWidget::regenerateRequested, this, &MainWindow::onGeneratePlan);
    connect(m_dashboard, &DashboardWidget::detailRequested, this, &MainWindow::onShowDetail);
    connect(m_dashboard, &DashboardWidget::mealDetailRequested, this, &MainWindow::onShowMealDetail);
    connect(m_dashboard, &DashboardWidget::favoriteToggled, this, &MainWindow::onFavoriteToggled);
    connect(m_dashboard, &DashboardWidget::openSettingsRequested, this, &MainWindow::onOpenSettings);

    connect(m_recommend, &RecommendWidget::generateRequested, this, [this]() {
        onGeneratePlan();
    });

    connect(m_recommend, &RecommendWidget::detailRequested, this, &MainWindow::onShowDetail);
    connect(m_recommend, &RecommendWidget::mealDetailRequested, this, &MainWindow::onShowMealDetail);
    connect(m_recommend, &RecommendWidget::favoriteToggled, this, &MainWindow::onFavoriteToggled);

    connect(m_recommend, &RecommendWidget::aiPreferenceApplied, this, [this](const AiPreferenceUpdate &update) {
        if (m_user.id <= 0)
            return;

        bool changed = false;
        if (!update.goal.isEmpty()
            && (update.goal == QLatin1String("lose") || update.goal == QLatin1String("gain")
                || update.goal == QLatin1String("maintain"))
            && update.goal != m_user.goal) {
            m_user.goal = update.goal;
            changed = true;
        }
        if (!update.preferences.isEmpty() && update.preferences != m_user.preferences) {
            m_user.preferences = update.preferences;
            changed = true;
        }
        if (!update.allergens.isEmpty() && update.allergens != m_user.allergens) {
            m_user.allergens = update.allergens;
            m_user.allergies = User::splitLegacyText(update.allergens);
            changed = true;
        }

        if (changed) {
            UserService svc;
            m_user.calorieTarget = svc.calculateDailyCalories(m_user);
            UserDAO dao;
            dao.updateUser(m_user);
            m_user = dao.findById(m_user.id);
            applyUser(m_user);
            statusBar()->showMessage(QStringLiteral("已根据 AI 建议更新饮食档案"), 4000);
        }

        // 仅档案确有变化，或模型明确要求重生成（且已通过问答过滤）时才重算方案
        if (changed || update.regenerate) {
            onGeneratePlan();
            onNavChanged(2);
            if (m_navButtons.size() > 2)
                m_navButtons[2]->setChecked(true);
        }
    });

    connect(m_profile, &ProfileWidget::detailRequested, this, &MainWindow::onShowDetail);
    connect(m_profile, &ProfileWidget::favoriteToggled, this, &MainWindow::onFavoriteToggled);
    connect(m_profile, &ProfileWidget::openSettingsRequested, this, &MainWindow::onOpenSettings);

    if (!m_navButtons.isEmpty()) {
        m_navButtons[0]->setChecked(true);
        m_stack->setCurrentIndex(0);
        m_topTitle->setText(QStringLiteral("今日膳食方案"));
    }
}

void MainWindow::startWithUser(const User &user)
{
    auto step = [](const char *msg) {
        const QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/startup.log");
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            f.write(msg);
            f.write("\n");
            f.flush();
        }
    };

    step("su_db");
    setDbConnected(DatabaseManager::getInstance().isOpen());
    if (!DatabaseManager::getInstance().isOpen()) {
        if (!openDatabase()) {
            QMessageBox::critical(this, QStringLiteral("数据库错误"),
                                  QStringLiteral("数据库未连接，主界面功能不可用。"));
            return;
        }
        setDbConnected(true);
    }

    step("su_apply");
    applyUser(user);
    step("su_nav");
    if (!m_navButtons.isEmpty()) {
        m_navButtons[0]->setChecked(true);
        if (m_stack)
            m_stack->setCurrentIndex(0);
        if (m_topTitle)
            m_topTitle->setText(QStringLiteral("今日膳食方案"));
    }

    // 方案 / 食材 / 收藏全部延后，避开登录对话框刚关闭时的事件风暴
    step("su_schedule");
    QTimer::singleShot(50, this, [this, step]() {
        step("su_plan");
        onGeneratePlan();
        step("su_foods");
        if (m_foods)
            m_foods->reload();
        step("su_fav");
        if (m_profile)
            m_profile->reloadFavorites();
        step("su_post_done");
    });
}

bool MainWindow::openDatabase()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/data/diet.db"),
        appDir + QStringLiteral("/../data/diet.db"),
        appDir + QStringLiteral("/../../data/diet.db"),
        QDir(appDir).absoluteFilePath(QStringLiteral("../data/diet.db")),
        QStringLiteral("data/diet.db"),
        QDir::current().absoluteFilePath(QStringLiteral("data/diet.db")),
    };

    // Also try source-tree relative path for debug runs from build dir
    const QString sourceGuess =
        QFileInfo(QStringLiteral("%1/../../data/diet.db").arg(appDir)).absoluteFilePath();
    QStringList paths = candidates;
    paths << sourceGuess;

    auto &dbm = DatabaseManager::getInstance();
    for (const QString &path : paths) {
        const QFileInfo fi(path);
        if (!fi.exists())
            continue;
        if (dbm.open(fi.absoluteFilePath()))
            return true;
    }

    // Last resort: create/open under appDir/data even if missing foods
    const QString fallback = appDir + QStringLiteral("/data/diet.db");
    QDir().mkpath(appDir + QStringLiteral("/data"));
    return dbm.open(fallback);
}

bool MainWindow::ensureLoggedIn()
{
    // Always show the entry page so a user can choose an existing local profile
    // or create a new plan instead of silently opening a demo account.
    LoginDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted && dlg.user().id > 0) {
        applyUser(dlg.user());
        return true;
    }
    return false;
}

void MainWindow::applyUser(const User &user)
{
    m_user = user;
    m_userService.setCurrentUserId(user.id);
    if (m_dashboard)
        m_dashboard->setUser(user);
    if (m_recommend)
        m_recommend->setUser(user);
    if (m_profile)
        m_profile->setUser(user);
    updateChrome();
}

void MainWindow::updateChrome()
{
    m_profileName->setText(m_user.name.isEmpty() ? QStringLiteral("未登录") : m_user.name);
    QString goalCn = QStringLiteral("维持");
    const QString g = m_user.goal.toLower();
    if (g == QLatin1String("lose"))
        goalCn = QStringLiteral("减重");
    else if (g == QLatin1String("gain"))
        goalCn = QStringLiteral("增肌");
    m_profileMeta->setText(QStringLiteral("%1 · %2 kcal")
                               .arg(goalCn)
                               .arg(m_user.calorieTarget));
}

void MainWindow::setDbConnected(bool ok)
{
    m_dbBadge->setProperty("connected", ok);
    m_dbBadge->setText(ok ? QStringLiteral("数据库已连接") : QStringLiteral("数据库未连接"));
    m_dbBadge->style()->unpolish(m_dbBadge);
    m_dbBadge->style()->polish(m_dbBadge);
}

void MainWindow::onNavChanged(int index)
{
    if (index < 0 || index >= m_stack->count())
        return;
    m_stack->setCurrentIndex(index);

    static const char *titles[] = {"今日膳食方案", "食材营养库", "智能推荐", "我的档案"};
    if (index >= 0 && index < 4)
        m_topTitle->setText(QString::fromUtf8(titles[index]));

    if (index == 1 && m_foods)
        m_foods->reload();
    if (index == 3 && m_profile)
        m_profile->reloadFavorites();
}

void MainWindow::onGeneratePlan()
{
    if (m_user.id <= 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先登录用户。"));
        return;
    }
    if (!DatabaseManager::getInstance().isOpen()) {
        QMessageBox::warning(this, QStringLiteral("数据库未连接"),
                             QStringLiteral("无法生成方案，请确认 SQLite 插件与 diet.db 可用。"));
        return;
    }

    RecommendEngine engine;
    const RecommendResult *prev = m_plan.valid ? &m_plan : nullptr;
    m_plan = engine.generatePlan(m_user, prev);
    if (m_dashboard) {
        m_dashboard->setUser(m_user);
        m_dashboard->setPlan(m_plan);
    }
    if (m_recommend)
        m_recommend->setPlan(m_plan);

    if (!m_plan.valid) {
        QMessageBox::warning(this, QStringLiteral("推荐失败"),
                             m_plan.summary.isEmpty()
                                 ? QStringLiteral("未能生成完整三餐方案。")
                                 : m_plan.summary);
        statusBar()->showMessage(QStringLiteral("推荐失败"), 3000);
    } else {
        statusBar()->showMessage(
            QStringLiteral("已生成：%1 ｜ %2 ｜ %3")
                .arg(m_plan.breakfast.title(), m_plan.lunch.title(), m_plan.dinner.title()),
            5000);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_profileMini && event->type() == QEvent::MouseButtonRelease) {
        onOpenSettings();
        return true;
    }
    if (watched == m_brandRow && event->type() == QEvent::MouseButtonRelease) {
        if (m_recommend) {
            const bool onRecommend = m_stack && m_stack->currentWidget() == m_recommend;
            if (!onRecommend) {
                if (m_navButtons.size() > 2)
                    m_navButtons[2]->setChecked(true);
                onNavChanged(2);
                // 从其他页点 Logo：进入智能推荐并确保助手可见
                m_recommend->setAiPanelVisible(true);
            } else {
                m_recommend->toggleAiPanel();
            }
        }
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::onShowDetail(const Recipe &recipe)
{
    RecipeDetailDialog dlg(recipe, this);
    dlg.exec();
}

void MainWindow::onShowMealDetail(const MealSlot &meal)
{
    RecipeDetailDialog dlg(meal, this);
    dlg.exec();
}

void MainWindow::onFavoriteToggled(int recipeId)
{
    if (m_user.id <= 0 || recipeId <= 0)
        return;
    RecipeDAO dao;
    dao.toggleFavorite(m_user.id, recipeId);
    m_dashboard->refreshFavorites(m_user.id);
    m_profile->reloadFavorites();
}

void MainWindow::onSwitchUser()
{
    LoginDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted && dlg.user().id > 0) {
        applyUser(dlg.user());
        onGeneratePlan();
    }
}

void MainWindow::onOpenSettings()
{
    if (m_user.id <= 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先登录用户。"));
        return;
    }
    SettingsDialog dlg(m_user, this);
    if (dlg.exec() == QDialog::Accepted) {
        applyUser(dlg.user());
        onGeneratePlan();
    }
}
