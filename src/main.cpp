#include "ui/MainWindow.h"
#include "ui/LoginDialog.h"
#include "ui/FavoritesWidget.h"
#include "ui/RecipeLibraryWidget.h"
#include "ui/FridgeWidget.h"
#include "ui/DietAnalyticsWidget.h"
#include "ui/HealthSyncDialog.h"
#include "ui/UiAssets.h"
#include "dao/DatabaseManager.h"
#include "dao/UserDAO.h"
#include "entities/Recipe.h"
#include "entities/User.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHash>
#include <QMessageBox>
#include <QMetaType>
#include <QMetaObject>
#include <QPixmap>
#include <QPainter>
#include <QPushButton>
#include <QTimer>

static void startupLog(const char *step)
{
    const QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/startup.log");
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        f.write(step);
        f.write("\n");
        f.flush();
    }
}

static void loadAppStyle(QApplication &app)
{
    QString qss;
    QFile res(QStringLiteral(":/styles.qss"));
    if (res.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qss = QString::fromUtf8(res.readAll());
        res.close();
    }
    if (qss.isEmpty()) {
        const QStringList candidates = {
            QCoreApplication::applicationDirPath() + QStringLiteral("/styles.qss"),
            QCoreApplication::applicationDirPath() + QStringLiteral("/resources/styles.qss"),
            QStringLiteral("resources/styles.qss"),
        };
        for (const QString &path : candidates) {
            QFile f(path);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                qss = QString::fromUtf8(f.readAll());
                f.close();
                break;
            }
        }
    }
    if (!qss.isEmpty()) {
        qss.replace(QStringLiteral("__SHANHENG_BODY_FONT__"), UiAssets::bodyFontFamily());
        qss.replace(QStringLiteral("__SHANHENG_TITLE_FONT__"), UiAssets::titleFontFamily());
        app.setStyleSheet(qss);
    }
}

static void applyAppFont(QApplication &app)
{
    app.setFont(UiAssets::bodyFont(14, QFont::Medium));
    app.setProperty("bodyFontFamily", UiAssets::bodyFontFamily());
    app.setProperty("titleFontFamily", UiAssets::titleFontFamily());
}

static bool openDatabaseEarly()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QCoreApplication::addLibraryPath(appDir);
    QCoreApplication::addLibraryPath(appDir + QStringLiteral("/plugins"));

    const QStringList candidates = {
        appDir + QStringLiteral("/data/diet.db"),
        appDir + QStringLiteral("/../data/diet.db"),
        QFileInfo(QStringLiteral("%1/../../data/diet.db").arg(appDir)).absoluteFilePath(),
        QDir::current().absoluteFilePath(QStringLiteral("data/diet.db")),
        QStringLiteral("data/diet.db"),
    };

    auto &dbm = DatabaseManager::getInstance();
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path) && dbm.open(QFileInfo(path).absoluteFilePath()))
            return true;
    }

    QDir().mkpath(appDir + QStringLiteral("/data"));
    return dbm.open(appDir + QStringLiteral("/data/diet.db"));
}

static User resolveStartupUser(int argc, char *argv[])
{
    QString loginName;
    QString loginPassword;
    bool autoLogin = false;

    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QLatin1String("--auto-login")) {
            autoLogin = true;
        } else if (arg == QLatin1String("--login") && i + 1 < argc) {
            loginName = QString::fromLocal8Bit(argv[++i]);
        } else if (arg == QLatin1String("--password") && i + 1 < argc) {
            loginPassword = QString::fromLocal8Bit(argv[++i]);
        }
    }

    UserDAO dao;
    if (!loginName.isEmpty()) {
        const User u = dao.authenticate(loginName, loginPassword);
        if (u.id > 0) {
            startupLog("cli_login_ok");
            return u;
        }
        startupLog("cli_login_fail");
        return User{};
    }

    // --smoke-test：默认用演示账号张明验证完整启动路径
    bool smokeTest = false;
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QLatin1String("--smoke-test"))
            smokeTest = true;
    }
    if (smokeTest) {
        const User u = dao.authenticate(QStringLiteral("张明"), QStringLiteral("123456"));
        if (u.id > 0) {
            startupLog("smoke_zhangming_ok");
            return u;
        }
        startupLog("smoke_zhangming_fail");
        const QList<User> users = dao.findAllUsers();
        if (!users.isEmpty())
            return users.first();
        return User{};
    }

    if (autoLogin) {
        const QList<User> users = dao.findAllUsers();
        if (!users.isEmpty())
            return users.first();
        return User{};
    }

    // 登录框在进入主窗口前完整销毁，避免与 MainWindow 生命周期重叠触发堆损坏检测
    LoginDialog loginDialog(nullptr);
    if (loginDialog.exec() != QDialog::Accepted)
        return User{};
    return loginDialog.user();
}

int main(int argc, char *argv[])
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("膳衡"));
    QApplication::setOrganizationName(QStringLiteral("SmartDiet"));

    // 全程关闭自动退出：由主窗口关闭时手动 quit，避免登录框销毁误杀进程
    QApplication::setQuitOnLastWindowClosed(false);

    applyAppFont(app);
    loadAppStyle(app);
    qRegisterMetaType<Recipe>("Recipe");

    QFile::remove(QCoreApplication::applicationDirPath() + QStringLiteral("/startup.log"));
    startupLog("boot");

    if (!openDatabaseEarly()) {
        startupLog("db_fail");
        QMessageBox::critical(
            nullptr,
            QStringLiteral("数据库错误"),
            QStringLiteral("无法打开 diet.db。\n请确认已部署 sqldrivers 插件，并存在 data/diet.db。"));
        return 1;
    }
    startupLog("db_ok");

    QString earlyReviewPage;
    QString earlyReviewOutput;
    int earlyReviewRecipeId = 469;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QLatin1String("--ui-review-page") && i + 1 < argc)
            earlyReviewPage = QString::fromLocal8Bit(argv[++i]).trimmed().toLower();
        else if (arg == QLatin1String("--ui-review-output") && i + 1 < argc)
            earlyReviewOutput = QString::fromLocal8Bit(argv[++i]).trimmed();
        else if (arg == QLatin1String("--ui-review-recipe-id") && i + 1 < argc) {
            bool ok = false;
            const int requestedId = QString::fromLocal8Bit(argv[++i]).toInt(&ok);
            if (ok && requestedId > 0)
                earlyReviewRecipeId = requestedId;
        }
    }

    // 登录/注册 01—06 独立验收，不进入主窗口，也不伪造登录状态。
    int authReviewState = -1;
    if (earlyReviewPage == QLatin1String("login")) {
        authReviewState = 0;
    } else if (earlyReviewPage.startsWith(QLatin1String("register"))) {
        bool ok = false;
        const int step = earlyReviewPage.mid(QStringLiteral("register").size()).toInt(&ok);
        if (ok && step >= 1 && step <= 5)
            authReviewState = step;
    }
    if (authReviewState >= 0) {
        LoginDialog reviewDialog(nullptr);
        reviewDialog.setReviewState(authReviewState);
        reviewDialog.show();
        if (!earlyReviewOutput.isEmpty()) {
            QTimer::singleShot(1200, &reviewDialog, [&reviewDialog, earlyReviewOutput]() {
                const QFileInfo outputInfo(earlyReviewOutput);
                QDir().mkpath(outputInfo.absolutePath());
                QPixmap shot(reviewDialog.size());
                shot.fill(Qt::transparent);
                reviewDialog.render(&shot);
                const bool saved = shot.save(outputInfo.absoluteFilePath(), "PNG");
                QCoreApplication::exit(saved ? 0 : 2);
            });
        }
        return app.exec();
    }

    const User user = resolveStartupUser(argc, argv);
    startupLog(user.id > 0 ? "login_ok" : "login_cancel");
    if (user.id <= 0)
        return 0;

    bool smokeTest = false;
    bool dnaCloseSmoke = false;
    bool fridgeCloseSmoke = false;
    QString uiReviewPage = earlyReviewPage;
    QString uiReviewOutput = earlyReviewOutput;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QLatin1String("--smoke-test"))
            smokeTest = true;
        else if (arg == QLatin1String("--dna-close-smoke"))
            dnaCloseSmoke = true;
        else if (arg == QLatin1String("--fridge-close-smoke"))
            fridgeCloseSmoke = true;
        else if ((arg == QLatin1String("--ui-review-page")
                  || arg == QLatin1String("--ui-review-output")
                  || arg == QLatin1String("--ui-review-recipe-id"))
                 && i + 1 < argc)
            ++i;
    }

    // 冒烟：模拟真实 UI 登录路径——构造并销毁含 TagChipGroup 的登录框
    if (smokeTest) {
        startupLog("dlg_create");
        {
            LoginDialog dlg(nullptr);
            dlg.resize(960, 640);
            dlg.show();
            startupLog("dlg_shown");
            QApplication::processEvents();
            startupLog("dlg_events_1");
            dlg.hide();
            startupLog("dlg_hidden");
            QApplication::processEvents();
            startupLog("dlg_events_2");
        }
        startupLog("dlg_destroyed");
    }

    startupLog("mainwindow_ctor");
    MainWindow window;
    window.setWindowTitle(QStringLiteral("膳衡 · 智能营养膳食推荐系统"));
    window.resize(1024, 768);
    window.setMinimumSize(960, 640);

    startupLog("mainwindow_show");
    window.show();
    window.raise();
    window.activateWindow();

    QTimer::singleShot(0, &window, [&window, user]() {
        startupLog("startWithUser");
        window.startWithUser(user);
        startupLog("startWithUser_done");
    });

    // 视觉验收入口：仅显式传参时切换真实页面，普通用户启动路径完全不受影响。
    // 示例：System.exe --auto-login --ui-review-page foods
    if (!uiReviewPage.isEmpty()) {
        QTimer::singleShot(900, &window, [&window, uiReviewPage, earlyReviewRecipeId]() {
            static const QHash<QString, int> pageIndexes = {
                {QStringLiteral("today"), 0},
                {QStringLiteral("foods"), 1},
                {QStringLiteral("recipes"), 2},
                {QStringLiteral("fridge"), 3},
                {QStringLiteral("fridge-add"), 3},
                {QStringLiteral("fridge-results"), 3},
                {QStringLiteral("recommend"), 4},
                {QStringLiteral("diet"), 5},
                {QStringLiteral("analytics"), 5},
                {QStringLiteral("health-sync"), 5},
                {QStringLiteral("favorites"), 6},
                {QStringLiteral("profile"), 7},
                {QStringLiteral("settings"), 7},
                {QStringLiteral("food-detail"), 1},
                {QStringLiteral("food-detail-favorite"), 1},
                {QStringLiteral("foods-usda-cn"), 1},
                {QStringLiteral("recipe-detail"), 0},
                {QStringLiteral("recipe-detail-favorite"), 0},
                {QStringLiteral("shopping-list"), 3},
                {QStringLiteral("recipe-import"), 2},
                {QStringLiteral("recipe-create"), 2},
                {QStringLiteral("food-vision"), 1},
                {QStringLiteral("ingredient-vision"), 1},
                {QStringLiteral("recipe-dna"), 2},
                {QStringLiteral("fridge-vision"), 3},
                {QStringLiteral("fridge-vision-initial"), 3},
                {QStringLiteral("fridge-vision-failure"), 3},
                {QStringLiteral("recommend-photo"), 4},
            };
            const auto it = pageIndexes.constFind(uiReviewPage);
            if (it == pageIndexes.cend())
                return;
            QMetaObject::invokeMethod(&window, "onNavChanged", Qt::DirectConnection,
                                      Q_ARG(int, it.value()));
            if (uiReviewPage == QLatin1String("settings")) {
                QTimer::singleShot(300, &window, [&window]() {
                    QMetaObject::invokeMethod(&window, "onOpenSettings", Qt::DirectConnection);
                });
            } else if (uiReviewPage == QLatin1String("diet")
                       || uiReviewPage == QLatin1String("analytics")) {
                QTimer::singleShot(250, &window, [&window]() {
                    if (auto *analytics = window.findChild<DietAnalyticsWidget *>())
                        analytics->setReviewState();
                });
            } else if (uiReviewPage == QLatin1String("foods-usda-cn")) {
                QTimer::singleShot(300, &window, [&window]() {
                    window.openFoodUsdaReview();
                });
            } else if (uiReviewPage == QLatin1String("food-detail")
                       || uiReviewPage == QLatin1String("food-detail-favorite")) {
                const bool favorite = uiReviewPage == QLatin1String("food-detail-favorite");
                QTimer::singleShot(300, &window, [&window, favorite]() {
                    window.openFoodReviewDetail(favorite);
                });
            } else if (uiReviewPage == QLatin1String("recipe-detail")
                       || uiReviewPage == QLatin1String("recipe-detail-favorite")) {
                const bool favorite = uiReviewPage == QLatin1String("recipe-detail-favorite");
                QTimer::singleShot(300, &window, [&window, favorite, earlyReviewRecipeId]() {
                    window.openRecipeReviewDetail(favorite, earlyReviewRecipeId);
                });
            } else if (uiReviewPage == QLatin1String("shopping-list")) {
                QTimer::singleShot(450, &window, [&window]() {
                    if (auto *fridge = window.findChild<FridgeWidget *>())
                        QMetaObject::invokeMethod(fridge, "onShoppingList", Qt::DirectConnection);
                });
            } else if (uiReviewPage == QLatin1String("fridge-add")) {
                QTimer::singleShot(300, &window, [&window]() {
                    if (auto *fridge = window.findChild<FridgeWidget *>()) {
                        for (QPushButton *button : fridge->findChildren<QPushButton *>()) {
                            if (button->text() == QStringLiteral("添加食材")) {
                                button->click();
                                break;
                            }
                        }
                    }
                });
            } else if (uiReviewPage == QLatin1String("fridge-results")) {
                QTimer::singleShot(350, &window, [&window]() {
                    if (auto *fridge = window.findChild<FridgeWidget *>())
                        QMetaObject::invokeMethod(fridge, "onRecommend", Qt::DirectConnection);
                });
            } else if (uiReviewPage == QLatin1String("recipe-import")
                       || uiReviewPage == QLatin1String("recipe-create")) {
                const bool web = uiReviewPage == QLatin1String("recipe-import");
                QTimer::singleShot(350, &window, [&window, web]() {
                    if (auto *library = window.findChild<RecipeLibraryWidget *>())
                        QMetaObject::invokeMethod(library,
                                                  web ? "openWebImporter" : "openManualCreator",
                                                  Qt::DirectConnection);
                });
            } else if (uiReviewPage == QLatin1String("health-sync")) {
                QTimer::singleShot(350, &window, [&window]() {
                    auto *dialog = new HealthSyncDialog(1, &window);
                    dialog->setAttribute(Qt::WA_DeleteOnClose);
                    dialog->setReviewState();
                    dialog->open();
                });
            } else if (uiReviewPage == QLatin1String("food-vision")) {
                QTimer::singleShot(350, &window, [&window]() {
                    window.openFoodVisionReview();
                });
            } else if (uiReviewPage == QLatin1String("ingredient-vision")) {
                QTimer::singleShot(350, &window, [&window]() {
                    window.openIngredientVisionReview();
                });
            } else if (uiReviewPage == QLatin1String("recipe-dna")) {
                QTimer::singleShot(350, &window, [&window, earlyReviewRecipeId]() {
                    window.openRecipeDnaReview(earlyReviewRecipeId);
                });
            } else if (uiReviewPage == QLatin1String("fridge-vision")) {
                QTimer::singleShot(350, &window, [&window]() {
                    window.openFridgeVisionReview();
                });
            } else if (uiReviewPage == QLatin1String("fridge-vision-initial")) {
                QTimer::singleShot(350, &window, [&window]() {
                    window.openFridgeVisionInitialReview();
                });
            } else if (uiReviewPage == QLatin1String("fridge-vision-failure")) {
                QTimer::singleShot(350, &window, [&window]() {
                    window.openFridgeVisionFailureReview();
                });
            } else if (uiReviewPage == QLatin1String("recommend-photo")) {
                QTimer::singleShot(350, &window, [&window]() {
                    window.openRecommendPhotoReview();
                });
            }
        });
    }

    if (dnaCloseSmoke) {
        QTimer::singleShot(1100, &window, [&window]() {
            startupLog("dna_close_smoke_begin");
            window.runRecipeDnaCloseSmoke();
            startupLog("dna_close_smoke_ok");
            QCoreApplication::exit(0);
        });
    }

    if (fridgeCloseSmoke) {
        QTimer::singleShot(1100, &window, [&window]() {
            startupLog("fridge_close_smoke_begin");
            window.runFridgeVisionCloseSmoke();
            startupLog("fridge_close_smoke_ok");
            QCoreApplication::exit(0);
        });
    }

    // 可重复的无干扰视觉验收：Qt 直接渲染窗口，不依赖桌面截屏或前台焦点。
    // 仅显式传入 --ui-review-output 时启用；普通启动路径不受影响。
    if (!uiReviewOutput.isEmpty()) {
        QTimer::singleShot(3500, &window, [&window, uiReviewOutput]() {
            const QFileInfo outputInfo(uiReviewOutput);
            QDir().mkpath(outputInfo.absolutePath());
            QPixmap shot(window.size());
            shot.fill(Qt::transparent);
            window.render(&shot);
            if (QWidget *modal = QApplication::activeModalWidget(); modal && modal != &window) {
                QPixmap modalShot(modal->size());
                modalShot.fill(Qt::transparent);
                modal->render(&modalShot);
                if (modal->width() > window.width() || modal->height() > window.height()) {
                    // 大尺寸编辑弹窗单独验收，避免被 1024×768 主窗口画布裁切。
                    shot = modalShot;
                } else {
                    const QPoint relative = modal->mapToGlobal(QPoint(0, 0))
                                            - window.mapToGlobal(QPoint(0, 0));
                    QPainter painter(&shot);
                    painter.fillRect(shot.rect(), QColor(245, 244, 241, 142));
                    painter.drawPixmap(relative, modalShot);
                }
            }
            const bool saved = shot.save(outputInfo.absoluteFilePath(), "PNG");
            startupLog(saved ? "ui_review_saved" : "ui_review_save_fail");
            QCoreApplication::exit(saved ? 0 : 2);
        });
    }

    // 关闭主窗口时退出（替代 quitOnLastWindowClosed，避免登录框销毁误杀进程）
    class CloseFilter final : public QObject {
    public:
        using QObject::QObject;
    protected:
        bool eventFilter(QObject *obj, QEvent *event) override
        {
            Q_UNUSED(obj);
            if (event->type() == QEvent::Close) {
                startupLog("main_close");
                QTimer::singleShot(0, qApp, &QApplication::quit);
            }
            return QObject::eventFilter(obj, event);
        }
    };
    window.installEventFilter(new CloseFilter(&window));

    // 自动化验证：登录后等待方案生成，确认无崩溃后退出
    if (smokeTest) {
        QTimer::singleShot(5000, &app, []() {
            startupLog("smoke_ok");
            QCoreApplication::exit(0);
        });
    }

    startupLog("exec");
    return app.exec();
}
