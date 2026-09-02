#include "MainWindow.h"

#include "DashboardWidget.h"
#include "DietAnalyticsWidget.h"
#include "FoodSearchWidget.h"
#include "IngredientVisionDialog.h"
#include "FavoritesWidget.h"
#include "RecipeLibraryWidget.h"
#include "FridgeWidget.h"
#include "FridgeVisionDialog.h"
#include "LoginDialog.h"
#include "LogoWidget.h"
#include "ProfileWidget.h"
#include "RecipeDetailDialog.h"
#include "FoodVisionDialog.h"
#include "RecipeDnaDialog.h"
#include "RecommendWidget.h"
#include "SettingsDialog.h"
#include "UiAssets.h"

#include "../dao/DatabaseManager.h"
#include "../dao/FoodDAO.h"
#include "../dao/FridgeDAO.h"
#include "../dao/RecipeDAO.h"
#include "../dao/UserDAO.h"
#include "../engine/RecommendEngine.h"
#include "../services/AiAssistantService.h"
#include "../services/AdaptiveTargetService.h"
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
#include <QIcon>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QString chineseDate(const QDate &date)
{
    static const QStringList weekdays = {
        QStringLiteral("星期一"), QStringLiteral("星期二"), QStringLiteral("星期三"),
        QStringLiteral("星期四"), QStringLiteral("星期五"), QStringLiteral("星期六"),
        QStringLiteral("星期日")};
    return QStringLiteral("%1年%2月%3日  ·  %4")
        .arg(date.year()).arg(date.month()).arg(date.day())
        .arg(weekdays.value(date.dayOfWeek() - 1));
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("膳衡 · 智能营养膳食推荐系统"));
    setWindowFlag(Qt::FramelessWindowHint, true);
    resize(1024, 768);
    setMinimumSize(960, 640);

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("CentralRoot"));
    setCentralWidget(central);

    auto *root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- Sidebar ----
    auto *sidebar = new QFrame(central);
    sidebar->setObjectName(QStringLiteral("Sidebar"));
    sidebar->setFixedWidth(208);
    sidebar->installEventFilter(this);
    m_sidebar = sidebar;
    auto *sideLay = new QVBoxLayout(sidebar);
    sideLay->setContentsMargins(15, 32, 13, 118);
    sideLay->setSpacing(7);

    auto *brandRow = new QWidget(sidebar);
    brandRow->setObjectName(QStringLiteral("BrandRow"));
    brandRow->setCursor(Qt::PointingHandCursor);
    brandRow->setToolTip(QStringLiteral("膳衡 · 智能营养膳食推荐系统"));
    m_brandRow = brandRow;
    auto *brandRowLayout = new QHBoxLayout(brandRow);
    brandRowLayout->setContentsMargins(2, 0, 2, 0);
    brandRowLayout->setSpacing(0);
    auto *brandLogo = new LogoWidget(brandRow);
    brandLogo->setFixedSize(170, 76);
    brandRow->setMinimumHeight(78);
    brandRowLayout->addWidget(brandLogo);
    brandRowLayout->addStretch();

    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);

    auto makeNav = [&](const QString &text, const QString &icon, int id) {
        // 避免给 QPushButton 嵌套 Layout（部分环境下易引发异常）
        auto *btn = new QPushButton(text, sidebar);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("class", QVariant(QStringLiteral("NavButton")));
        btn->setProperty("iconName", icon);
        btn->setMinimumHeight(42);
        UiAssets::setButtonIcon(btn, icon, 24);
        m_navGroup->addButton(btn, id);
        m_navButtons.append(btn);
        sideLay->addWidget(btn);
        return btn;
    };

    sideLay->addWidget(brandRow);
    sideLay->addSpacing(26);
    makeNav(QStringLiteral("今日方案"), QStringLiteral("calendar"), TodayPage);
    makeNav(QStringLiteral("食材库"), QStringLiteral("ingredient-bag"), FoodPage);
    makeNav(QStringLiteral("食谱大全"), QStringLiteral("category-grid"), RecipeLibraryPage);
    makeNav(QStringLiteral("冰箱库存"), QStringLiteral("fridge"), FridgePage);
    makeNav(QStringLiteral("智能推荐"), QStringLiteral("recommend-star"), RecommendPage);
    makeNav(QStringLiteral("饮食分析"), QStringLiteral("chart-line"), DietAnalyticsPage);
    makeNav(QStringLiteral("我的收藏"), QStringLiteral("heart"), FavoritesPage);
    makeNav(QStringLiteral("我的档案"), QStringLiteral("user"), ProfilePage);
    sideLay->addStretch();

    auto *profileMini = new QFrame(sidebar);
    profileMini->setObjectName(QStringLiteral("ProfileMini"));
    profileMini->setCursor(Qt::PointingHandCursor);
    profileMini->setToolTip(QStringLiteral("点击打开用户设置"));
    m_profileMini = profileMini;
    profileMini->setFixedHeight(50);
    auto *pmLay = new QHBoxLayout(profileMini);
    pmLay->setContentsMargins(12, 9, 12, 9);
    m_profileAvatar = new QLabel(QStringLiteral("膳"), profileMini);
    m_profileAvatar->setObjectName(QStringLiteral("ProfileMiniAvatar"));
    m_profileAvatar->setAlignment(Qt::AlignCenter);
    auto *pmText = new QVBoxLayout;
    pmText->setSpacing(1);
    m_profileName = new QLabel(QStringLiteral("未登录"), profileMini);
    m_profileName->setObjectName(QStringLiteral("ProfileMiniName"));
    m_profileMeta = new QLabel(QStringLiteral("点击打开用户设置"), profileMini);
    m_profileMeta->setObjectName(QStringLiteral("ProfileMiniMeta"));
    m_profileMeta->setWordWrap(true);
    pmText->addWidget(m_profileName);
    pmText->addWidget(m_profileMeta);
    pmLay->addWidget(m_profileAvatar);
    pmLay->addLayout(pmText, 1);
    auto *arrow = UiAssets::createIconLabel(profileMini, QStringLiteral("chevron-right"), 18);
    arrow->setObjectName(QStringLiteral("ProfileMiniArrow"));
    pmLay->addWidget(arrow);

    sideLay->addWidget(profileMini);

    m_sidebarDecoration = new QLabel(sidebar);
    m_sidebarDecoration->setObjectName(QStringLiteral("SidebarDecoration"));
    m_sidebarDecoration->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_sidebarDecoration->setPixmap(UiAssets::svgPixmap(
        QStringLiteral("sidebar-botanical"), QSize(208, 145), QColor(), m_sidebarDecoration));
    m_sidebarDecoration->setFixedSize(208, 145);
    m_sidebarDecoration->move(0, qMax(0, sidebar->height() - 145));
    m_sidebarDecoration->lower();

    brandRow->installEventFilter(this);
    profileMini->installEventFilter(this);

    // ---- Right column ----
    auto *right = new QWidget(central);
    auto *rightLay = new QVBoxLayout(right);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(0);

    m_titleBar = new QFrame(right);
    m_titleBar->setObjectName(QStringLiteral("WindowTitleBar"));
    m_titleBar->installEventFilter(this);
    auto *windowControls = new QHBoxLayout(m_titleBar);
    windowControls->setContentsMargins(0, 0, 7, 0);
    windowControls->setSpacing(0);
    windowControls->addStretch();
    auto makeWindowButton = [this](const QString &name) {
        auto *button = new QPushButton(m_titleBar);
        button->setObjectName(name);
        button->setFixedSize(42, 35);
        button->setCursor(Qt::PointingHandCursor);
        return button;
    };
    auto *minButton = makeWindowButton(QStringLiteral("WindowMinButton"));
    auto *maxButton = makeWindowButton(QStringLiteral("WindowMaxButton"));
    auto *closeButton = makeWindowButton(QStringLiteral("WindowCloseButton"));
    minButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMinButton));
    maxButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
    UiAssets::setButtonIcon(closeButton, QStringLiteral("close"), 18);
    minButton->setIconSize(QSize(16, 16));
    maxButton->setIconSize(QSize(16, 16));
    windowControls->addWidget(minButton);
    windowControls->addWidget(maxButton);
    windowControls->addWidget(closeButton);
    connect(minButton, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(maxButton, &QPushButton::clicked, this, [this]() { isMaximized() ? showNormal() : showMaximized(); });
    connect(closeButton, &QPushButton::clicked, this, &QWidget::close);

    auto *topBar = new QFrame(right);
    topBar->setObjectName(QStringLiteral("TopBar"));
    m_topBar = topBar;
    topBar->setFixedHeight(119);
    auto *topLay = new QHBoxLayout(topBar);
    topLay->setContentsMargins(32, 14, 24, 14);

    auto *titleBox = new QVBoxLayout;
    titleBox->setSpacing(2);
    m_topDate = new QLabel(chineseDate(QDate::currentDate()), topBar);
    m_topDate->setObjectName(QStringLiteral("TopDate"));
    m_topTitle = new QLabel(QStringLiteral("今日方案"), topBar);
    m_topTitle->setObjectName(QStringLiteral("TopTitle"));
    m_topTitle->setFont(UiAssets::titleFont(31));
    m_topTitle->setMinimumHeight(42);
    m_topSubtitle = new QLabel(QStringLiteral("根据你的身体数据，为你安排今天的三餐。"), topBar);
    m_topSubtitle->setObjectName(QStringLiteral("TopSubtitle"));
    titleBox->addWidget(m_topDate);
    titleBox->addSpacing(7);
    titleBox->addWidget(m_topTitle);
    titleBox->addSpacing(3);
    titleBox->addWidget(m_topSubtitle);

    m_dbBadge = new QLabel(QStringLiteral("数据库未连接"), topBar);
    m_dbBadge->setObjectName(QStringLiteral("DbBadge"));
    m_dbBadge->setProperty("connected", false);

    auto *switchBtn = new QPushButton(QStringLiteral("切换用户"), topBar);
    switchBtn->setObjectName(QStringLiteral("SwitchUserBtn"));
    switchBtn->setCursor(Qt::PointingHandCursor);

    m_recommendGoalTags = new QWidget(topBar);
    m_recommendGoalTags->setFixedHeight(80);
    auto *goalTagLay = new QHBoxLayout(m_recommendGoalTags);
    goalTagLay->setContentsMargins(0, 48, 0, 0);
    goalTagLay->setSpacing(10);
    auto makeGoalTag = [this, topBar](const QString &text, const QString &iconName,
                                      const QString &tone, const QColor &color) {
        auto *tag = new QPushButton(text, topBar);
        tag->setObjectName(QStringLiteral("RecommendGoalTag"));
        tag->setProperty("tone", tone);
        tag->setAttribute(Qt::WA_TransparentForMouseEvents);
        UiAssets::setButtonIcon(tag, iconName, 18, color);
        return tag;
    };
    m_recommendGoalTag = makeGoalTag(QStringLiteral("维持目标"), QStringLiteral("target"),
                                     QStringLiteral("green"), QColor(QStringLiteral("#16A765")));
    goalTagLay->addWidget(m_recommendGoalTag);
    goalTagLay->addWidget(makeGoalTag(QStringLiteral("高蛋白"), QStringLiteral("protein"),
                                      QStringLiteral("blue"), QColor(QStringLiteral("#3B82F6"))));
    goalTagLay->addWidget(makeGoalTag(QStringLiteral("少油"), QStringLiteral("flame"),
                                      QStringLiteral("orange"), QColor(QStringLiteral("#F59E0B"))));
    m_recommendGoalTags->hide();

    topLay->addLayout(titleBox);
    topLay->addStretch();
    m_dbBadge->hide();
    topLay->addWidget(m_recommendGoalTags, 0, Qt::AlignTop);
    topLay->addWidget(switchBtn);

    m_topDecoration = new QLabel(topBar);
    m_topDecoration->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_topDecoration->setPixmap(UiAssets::svgPixmap(
        QStringLiteral("leaf-watermark"), QSize(112, 82), QColor(), m_topDecoration));
    m_topDecoration->setFixedSize(112, 82);
    m_topDecoration->move(690, 5);
    m_topDecoration->lower();

    m_stack = new QStackedWidget(right);
    m_dashboard = new DashboardWidget(m_stack);
    m_foods = new FoodSearchWidget(m_stack);
    m_recipeLibrary = new RecipeLibraryWidget(m_stack);
    m_fridge = new FridgeWidget(m_stack);
    m_recommend = new RecommendWidget(m_stack);
    m_dietAnalytics = new DietAnalyticsWidget(m_stack);
    m_favorites = new FavoritesWidget(m_stack);
    m_profile = new ProfileWidget(m_stack);
    m_stack->addWidget(m_dashboard); // 0
    m_stack->addWidget(m_foods);     // 1
    m_stack->addWidget(m_recipeLibrary); // 2
    m_stack->addWidget(m_fridge);    // 3
    m_stack->addWidget(m_recommend); // 4
    m_stack->addWidget(m_dietAnalytics); // 5
    m_stack->addWidget(m_favorites); // 6
    m_stack->addWidget(m_profile);   // 7

    auto *footer = new QLabel(
        QStringLiteral("免责声明：膳衡提供的膳食建议仅供参考，不能替代医师或注册营养师的专业意见。"),
        right);
    footer->setObjectName(QStringLiteral("FooterDisclaimer"));
    footer->setWordWrap(true);
    footer->setAlignment(Qt::AlignCenter);

    rightLay->addWidget(m_titleBar);
    rightLay->addWidget(topBar);
    rightLay->addWidget(m_stack, 1);
    footer->hide();

    root->addWidget(sidebar);
    root->addWidget(right, 1);

    connect(m_navGroup, &QButtonGroup::idClicked, this, &MainWindow::onNavChanged);
    connect(switchBtn, &QPushButton::clicked, this, &MainWindow::onSwitchUser);

    connect(m_dashboard, &DashboardWidget::regenerateRequested, this, &MainWindow::onGeneratePlan);
    connect(m_dashboard, &DashboardWidget::detailRequested, this, &MainWindow::onShowDetail);
    connect(m_dashboard, &DashboardWidget::mealDetailRequested, this, &MainWindow::onShowMealDetail);
    connect(m_dashboard, &DashboardWidget::favoriteToggled, this, &MainWindow::onFavoriteToggled);
    connect(m_dashboard, &DashboardWidget::openSettingsRequested, this, &MainWindow::onOpenSettings);
    connect(m_foods, &FoodSearchWidget::foodFavoriteChanged, this, [this]() {
        if (m_favorites)
            m_favorites->reload();
        if (m_profile)
            m_profile->reloadFavorites();
        statusBar()->showMessage(QStringLiteral("食材收藏已更新"), 2500);
    });

    connect(m_recommend, &RecommendWidget::generateRequested, this, [this]() {
        onGeneratePlan();
    });

    connect(m_recommend, &RecommendWidget::detailRequested, this, &MainWindow::onShowDetail);
    connect(m_recommend, &RecommendWidget::mealDetailRequested, this, &MainWindow::onShowMealDetail);
    connect(m_recommend, &RecommendWidget::favoriteToggled, this, &MainWindow::onFavoriteToggled);
    connect(m_recommend, &RecommendWidget::todayPlanRequested, this, [this]() {
        onNavChanged(TodayPage);
        if (!m_navButtons.isEmpty())
            m_navButtons[TodayPage]->setChecked(true);
        statusBar()->showMessage(QStringLiteral("已打开最新今日方案"), 3000);
    });
    connect(m_dietAnalytics, &DietAnalyticsWidget::foodLogSaved, this, [this](int logId) {
        statusBar()->showMessage(
            QStringLiteral("已自动写入饮食记录（编号 %1）").arg(logId), 3500);
    });

    connect(m_fridge, &FridgeWidget::detailRequested, this, &MainWindow::onShowDetail);
    connect(m_fridge, &FridgeWidget::mealDetailRequested, this, &MainWindow::onShowMealDetail);
    connect(m_fridge, &FridgeWidget::planGenerated, this, [this](const RecommendResult &plan) {
        if (!plan.valid)
            return;
        m_plan = plan;
        if (m_dashboard) {
            m_dashboard->setUser(m_user);
            m_dashboard->setPlan(m_plan);
        }
        if (m_recommend)
            m_recommend->setPlan(m_plan);
        if (m_fridge)
            m_fridge->setPlan(m_plan);
        statusBar()->showMessage(QStringLiteral("清冰箱三餐已同步到今日方案"), 5000);
    });

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
            onNavChanged(RecommendPage);
            if (m_navButtons.size() > RecommendPage)
                m_navButtons[RecommendPage]->setChecked(true);
        }
    });

    connect(m_profile, &ProfileWidget::detailRequested, this, &MainWindow::onShowDetail);
    connect(m_profile, &ProfileWidget::favoriteToggled, this, &MainWindow::onFavoriteToggled);
    connect(m_profile, &ProfileWidget::openSettingsRequested, this, &MainWindow::onOpenSettings);
    connect(m_profile, &ProfileWidget::openFavoritesRequested, this, [this]() {
        onNavChanged(FavoritesPage);
        if (m_navButtons.size() > FavoritesPage)
            m_navButtons[FavoritesPage]->setChecked(true);
    });
    connect(m_recipeLibrary, &RecipeLibraryWidget::detailRequested,
            this, &MainWindow::onShowDetail);
    connect(m_recipeLibrary, &RecipeLibraryWidget::favoriteToggled,
            this, &MainWindow::onFavoriteToggled);
    connect(m_recipeLibrary, &RecipeLibraryWidget::personalRecipeCreated,
            this, [this](int recipeId) {
        refreshFavoriteViews();
        statusBar()->showMessage(
            QStringLiteral("个人食谱已保存并加入食谱大全（编号 %1）").arg(recipeId), 5000);
    });
    connect(m_favorites, &FavoritesWidget::detailRequested, this, &MainWindow::onShowDetail);
    connect(m_favorites, &FavoritesWidget::favoriteToggled, this, &MainWindow::onFavoriteToggled);
    connect(m_favorites, &FavoritesWidget::foodFavoriteToggled, this, [this](int foodId) {
        if (m_user.id <= 0 || foodId <= 0)
            return;
        FoodDAO dao;
        if (!dao.setFavorite(m_user.id, foodId, false)) {
            QMessageBox::warning(this, QStringLiteral("收藏失败"),
                                 QStringLiteral("收藏状态未保存，请稍后重试。"));
            return;
        }
        QTimer::singleShot(0, this, [this]() { refreshFavoriteViews(); });
    });
    connect(m_favorites, &FavoritesWidget::personalRecipeCreated, this, [this](int recipeId) {
        refreshFavoriteViews();
        statusBar()->showMessage(QStringLiteral("个人食谱已保存并加入收藏（编号 %1）").arg(recipeId), 5000);
    });

    if (!m_navButtons.isEmpty()) {
        onNavChanged(TodayPage);
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
        m_navButtons[TodayPage]->setChecked(true);
        if (m_stack)
            m_stack->setCurrentIndex(TodayPage);
        if (m_topTitle)
            m_topTitle->setText(QStringLiteral("今日方案"));
    }

    // 首屏只生成今日方案；食材、冰箱、收藏在首次进入各页时按需加载，
    // 避免登录后同时扫描数据库和创建大量图片控件导致界面卡顿。
    step("su_schedule");
    QTimer::singleShot(50, this, [this, step]() {
        step("su_plan");
        try {
            onGeneratePlan();
        } catch (...) {
            step("su_plan_throw");
        }
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
    if (m_dashboard) {
        const AdaptiveTargetResult adaptive = AdaptiveTargetService().analyze(user, 14);
        User effectiveUser = user;
        effectiveUser.calorieTarget = adaptive.effectiveTarget;
        m_dashboard->setAdaptiveTarget(adaptive);
        m_dashboard->setUser(effectiveUser);
    }
    if (m_foods)
        m_foods->setUserId(user.id);
    if (m_recipeLibrary)
        m_recipeLibrary->setUser(user);
    if (m_recommend)
        m_recommend->setUser(user);
    if (m_dietAnalytics)
        m_dietAnalytics->setUser(user);
    // 冰箱仅在进入页或延后任务中 reload，避免登录瞬时重复 IO
    if (m_fridge)
        m_fridge->setUser(user);
    if (m_profile)
        m_profile->setUser(user);
    if (m_favorites)
        m_favorites->setUser(user);
    updateChrome();

    int expiring = 0;
    int expired = 0;
    const QDate today = QDate::currentDate();
    for (const FridgeItem &item : FridgeDAO().listByUser(user.id)) {
        const QDate expiry = QDate::fromString(item.expiryDate, Qt::ISODate);
        if (!expiry.isValid())
            continue;
        if (expiry < today)
            ++expired;
        else if (today.daysTo(expiry) <= 3)
            ++expiring;
    }
    if (expiring > 0 || expired > 0) {
        QTimer::singleShot(900, this, [this, expiring, expired]() {
            statusBar()->showMessage(
                QStringLiteral("冰箱提醒：%1 种食材即将过期，%2 种已过期；可在冰箱库存查看推荐。")
                    .arg(expiring).arg(expired), 12000);
        });
    }
}

void MainWindow::updateChrome()
{
    m_profileName->setText(m_user.name.isEmpty() ? QStringLiteral("未登录") : m_user.name);
    if (m_profileAvatar) {
        const QString name = m_user.name.trimmed();
        m_profileAvatar->setText(name.isEmpty() ? QStringLiteral("膳") : name.left(1));
    }
    QString goalCn = QStringLiteral("维持");
    const QString g = m_user.goal.toLower();
    if (g == QLatin1String("lose"))
        goalCn = QStringLiteral("减重");
    else if (g == QLatin1String("gain"))
        goalCn = QStringLiteral("增肌");
    m_profileMeta->setText(QStringLiteral("%1 · %2 kcal")
                               .arg(goalCn)
                               .arg(m_user.calorieTarget));
    if (m_recommendGoalTag)
        m_recommendGoalTag->setText(QStringLiteral("%1目标").arg(goalCn));
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
    for (int i = 0; i < m_navButtons.size(); ++i) {
        m_navButtons[i]->setChecked(i == index);
        UiAssets::setButtonIcon(m_navButtons[i],
                                m_navButtons[i]->property("iconName").toString(), 24,
                                i == index ? QColor(QStringLiteral("#059669"))
                                           : QColor(QStringLiteral("#0B163A")));
    }

    static const char *titles[] = {
        "今日方案", "食材库", "食谱大全", "冰箱库存", "智能推荐", "饮食记录与分析", "我的收藏", "我的档案"};
    static const char *subtitles[] = {
        "多摄入深绿色蔬菜，有助于补充膳食纤维和维生素。",
        "按类别查看常见食材营养数据",
        "浏览分类食谱，也可以导入或创建自己的家庭菜谱。",
        "查看冰箱中食材的库存与保质状态，合理规划饮食，减少浪费",
        "和膳衡聊聊你的饮食需求。",
        "识别多道菜品，自动记录摄入并分析近期营养趋势。",
        "收藏的食谱和食材会保存在本地档案中。",
        "查看身体数据、饮食偏好和健康信息"
    };
    if (index >= 0 && index < 8) {
        m_topTitle->setText(QString::fromUtf8(titles[index]));
        m_topSubtitle->setText(QString::fromUtf8(subtitles[index]));
    }

    const bool showDate = index == TodayPage || index == FoodPage
                          || index == RecipeLibraryPage || index == DietAnalyticsPage
                          || index == FavoritesPage
                          || index == ProfilePage;
    m_topDate->setVisible(showDate);
    m_topSubtitle->setVisible(index != ProfilePage);
    if (auto *switchBtn = findChild<QPushButton *>(QStringLiteral("SwitchUserBtn")))
        switchBtn->setVisible(index == FoodPage || index == FavoritesPage || index == ProfilePage);
    if (m_topDecoration) {
        if (index == TodayPage) {
            m_topDecoration->setFixedSize(112, 82);
            m_topDecoration->move(690, 5);
            m_topDecoration->setPixmap(UiAssets::svgPixmap(
                QStringLiteral("leaf-watermark"), QSize(112, 82), QColor(), m_topDecoration));
            m_topDecoration->show();
        } else if (index == FridgePage) {
            m_topDecoration->setFixedSize(42, 42);
            m_topDecoration->move(756, 20);
            m_topDecoration->setPixmap(UiAssets::svgPixmap(
                QStringLiteral("fridge"), QSize(40, 40), QColor(QStringLiteral("#14213D")),
                m_topDecoration));
            m_topDecoration->show();
        } else {
            m_topDecoration->hide();
        }
    }
    if (m_topBar) {
        m_topBar->setProperty("compact", index == ProfilePage);
        m_topBar->style()->unpolish(m_topBar);
        m_topBar->style()->polish(m_topBar);
        m_topBar->setFixedHeight(index == ProfilePage ? 96 : 119);
    }
    if (m_stack)
        m_stack->setFocus(Qt::OtherFocusReason);
    if (m_recommendGoalTags)
        m_recommendGoalTags->setVisible(index == RecommendPage);

    if (index == FoodPage && m_foods)
        m_foods->reload();
    if (index == RecipeLibraryPage && m_recipeLibrary)
        m_recipeLibrary->reload();
    if (index == FridgePage && m_fridge)
        m_fridge->reload();
    if (index == DietAnalyticsPage && m_dietAnalytics)
        m_dietAnalytics->reload();
    if (index == FavoritesPage && m_favorites)
        m_favorites->reload();
    if (index == ProfilePage && m_profile)
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
    const AdaptiveTargetResult adaptive = AdaptiveTargetService().analyze(m_user, 14);
    User effectiveUser = m_user;
    effectiveUser.calorieTarget = adaptive.effectiveTarget;
    const RecommendResult *prev = m_plan.valid ? &m_plan : nullptr;
    m_plan = engine.generatePlan(effectiveUser, prev);
    if (m_dashboard) {
        m_dashboard->setAdaptiveTarget(adaptive);
        m_dashboard->setUser(effectiveUser);
        m_dashboard->setPlan(m_plan);
    }
    if (m_recommend)
        m_recommend->setPlan(m_plan);
    if (m_fridge)
        m_fridge->setPlan(m_plan);

    if (!m_plan.valid) {
        QMessageBox::warning(this, QStringLiteral("推荐失败"),
                             m_plan.summary.isEmpty()
                                 ? QStringLiteral("未能生成完整三餐方案。")
                                 : m_plan.summary);
        statusBar()->showMessage(QStringLiteral("推荐失败"), 3000);
    } else {
        statusBar()->showMessage(QStringLiteral("今日膳食方案已更新"), 5000);
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_sidebar && event->type() == QEvent::Resize && m_sidebarDecoration) {
        m_sidebarDecoration->move(0, qMax(0, m_sidebar->height() - m_sidebarDecoration->height()));
        m_sidebarDecoration->lower();
    }
    if (watched == m_titleBar) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                m_dragOffset = mouse->globalPosition().toPoint() - frameGeometry().topLeft();
                m_dragging = true;
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_dragging && !isMaximized()) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            move(mouse->globalPosition().toPoint() - m_dragOffset);
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_dragging = false;
            return true;
        }
    }
    if (watched == m_profileMini && event->type() == QEvent::MouseButtonRelease) {
        onOpenSettings();
        return true;
    }
    if (watched == m_brandRow && event->type() == QEvent::MouseButtonRelease) {
        if (m_recommend) {
            const bool onRecommend = m_stack && m_stack->currentWidget() == m_recommend;
            if (!onRecommend) {
                if (m_navButtons.size() > RecommendPage)
                    m_navButtons[RecommendPage]->setChecked(true);
                onNavChanged(RecommendPage);
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
    RecipeDetailDialog dlg(recipe, m_user.id, this);
    connect(&dlg, &RecipeDetailDialog::favoriteChanged, this,
            [this](int, bool) { refreshFavoriteViews(); });
    connect(&dlg, &RecipeDetailDialog::personalRecipeCreated, this, [this](int recipeId) {
        refreshFavoriteViews();
        statusBar()->showMessage(
            QStringLiteral("DNA改造食谱已加入食谱大全（编号 %1）").arg(recipeId), 5000);
    });
    dlg.exec();
}

void MainWindow::onShowMealDetail(const MealSlot &meal)
{
    RecipeDetailDialog dlg(meal, m_user.id, this);
    connect(&dlg, &RecipeDetailDialog::favoriteChanged, this,
            [this](int, bool) { refreshFavoriteViews(); });
    connect(&dlg, &RecipeDetailDialog::personalRecipeCreated, this, [this](int recipeId) {
        refreshFavoriteViews();
        statusBar()->showMessage(
            QStringLiteral("DNA改造食谱已加入食谱大全（编号 %1）").arg(recipeId), 5000);
    });
    dlg.exec();
}

void MainWindow::onFavoriteToggled(int recipeId)
{
    if (m_user.id <= 0 || recipeId <= 0)
        return;
    RecipeDAO dao;
    if (!dao.toggleFavorite(m_user.id, recipeId)) {
        QMessageBox::warning(this, QStringLiteral("收藏失败"),
                             QStringLiteral("收藏状态未保存，请稍后重试。"));
        return;
    }
    QTimer::singleShot(0, this, [this]() { refreshFavoriteViews(); });
}

void MainWindow::openFoodReviewDetail(bool favoriteState)
{
    if (m_foods)
        m_foods->openReviewDetail(favoriteState);
}

void MainWindow::openFoodUsdaReview()
{
    if (m_foods)
        m_foods->setUsdaReviewState();
}

void MainWindow::openRecipeReviewDetail(bool favoriteState, int recipeId)
{
    Recipe recipe = RecipeDAO().findById(recipeId);
    for (const MealSlot *meal : {&m_plan.lunch, &m_plan.breakfast, &m_plan.dinner}) {
        if (!recipe.isValid() && meal && !meal->dishes.isEmpty()) {
            recipe = meal->dishes.first();
            break;
        }
    }
    if (!recipe.isValid()) {
        const QList<Recipe> recipes = RecipeDAO().findAll();
        if (!recipes.isEmpty())
            recipe = recipes.first();
    }
    if (!recipe.isValid())
        return;
    RecipeDetailDialog dialog(recipe, m_user.id, this, favoriteState ? 1 : 0);
    dialog.exec();
}

void MainWindow::openFoodVisionReview()
{
    const QStringList candidates = {
        QCoreApplication::applicationDirPath()
            + QStringLiteral("/data/recipe_images/0301_鱼香茄子_335fe4e7.jpg"),
        QDir::currentPath()
            + QStringLiteral("/食谱数据/图片工作区/assets/recipes/complete/0301_鱼香茄子_335fe4e7.jpg"),
    };
    QString imagePath;
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            imagePath = candidate;
            break;
        }
    }
    FoodVisionDialog dialog(m_user.id, this);
    dialog.setReviewState(imagePath);
    dialog.exec();
}

void MainWindow::openIngredientVisionReview()
{
    const QStringList candidates = {
        QCoreApplication::applicationDirPath()
            + QStringLiteral("/data/recipe_images/0301_鱼香茄子_335fe4e7.jpg"),
        QDir::currentPath()
            + QStringLiteral("/食谱数据/图片工作区/assets/recipes/complete/0301_鱼香茄子_335fe4e7.jpg"),
    };
    QString imagePath;
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            imagePath = candidate;
            break;
        }
    }
    IngredientVisionDialog dialog(m_user.id, this);
    dialog.setReviewState(imagePath);
    dialog.exec();
}

void MainWindow::openRecipeDnaReview(int recipeId)
{
    Recipe recipe = RecipeDAO().findById(recipeId);
    if (!recipe.isValid()) {
        const QList<Recipe> recipes = RecipeDAO().findAll();
        if (!recipes.isEmpty())
            recipe = recipes.first();
    }
    if (!recipe.isValid())
        return;
    RecipeDnaDialog dialog(recipe, m_user.id, this);
    dialog.setReviewState();
    dialog.exec();
}

void MainWindow::runRecipeDnaCloseSmoke()
{
    Recipe recipe;
    const QList<Recipe> recipes = RecipeDAO().findAll();
    if (!recipes.isEmpty())
        recipe = recipes.first();
    if (!recipe.isValid())
        return;

    // 覆盖已完成结果关闭，以及活动网络请求被立即关闭两条析构路径。
    for (int i = 0; i < 12; ++i) {
        RecipeDnaDialog dialog(recipe, m_user.id, this);
        dialog.setReviewState();
        dialog.show();
        QApplication::processEvents();
        dialog.reject();
        QApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    RecipeDnaDialog activeDialog(recipe, m_user.id, this);
    activeDialog.show();
    activeDialog.requestTransform(QStringLiteral("提高蛋白质，热量不要明显上升"));
    QTimer::singleShot(80, &activeDialog, &RecipeDnaDialog::reject);
    activeDialog.exec();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

void MainWindow::openFridgeVisionReview()
{
    const QStringList candidates = {
        QCoreApplication::applicationDirPath()
            + QStringLiteral("/data/recipe_images/0301_鱼香茄子_335fe4e7.jpg"),
        QDir::currentPath()
            + QStringLiteral("/食谱数据/图片工作区/assets/recipes/complete/0301_鱼香茄子_335fe4e7.jpg"),
    };
    QString imagePath;
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            imagePath = candidate;
            break;
        }
    }
    FridgeVisionDialog *dialog = FridgeVisionDialog::create(m_user.id, this);
    dialog->setReviewState(imagePath);
    dialog->exec();
    delete dialog;
}

void MainWindow::openFridgeVisionInitialReview()
{
    FridgeVisionDialog *dialog = FridgeVisionDialog::create(m_user.id, this);
    dialog->exec();
    delete dialog;
}

void MainWindow::openFridgeVisionFailureReview()
{
    FridgeVisionDialog *dialog = FridgeVisionDialog::create(m_user.id, this);
    dialog->setFailureReviewState();
    dialog->exec();
    delete dialog;
}

void MainWindow::runFridgeVisionCloseSmoke()
{
    // 回归“识别失败后关闭”路径：反复创建、失败、关闭和释放，MSVC 的
    // /RTC 栈检查与 Debug Heap 会在此捕获越界或生命周期错误。
    for (int i = 0; i < 20; ++i) {
        FridgeVisionDialog *dialog = FridgeVisionDialog::create(m_user.id, this);
        dialog->setFailureReviewState();
        dialog->show();
        QApplication::processEvents();
        dialog->reject();
        QApplication::processEvents();
        delete dialog;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
}

void MainWindow::openRecommendPhotoReview()
{
    if (m_recommend)
        m_recommend->setPhotoReviewState();
}

void MainWindow::refreshFavoriteViews()
{
    m_dashboard->refreshFavorites(m_user.id);
    m_profile->reloadFavorites();
    if (m_recipeLibrary)
        m_recipeLibrary->reload();
    if (m_favorites)
        m_favorites->reload();
    if (m_foods)
        m_foods->reload();
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
