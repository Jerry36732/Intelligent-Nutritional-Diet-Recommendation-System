#include "ui/MainWindow.h"
#include "ui/LoginDialog.h"
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
#include <QMessageBox>
#include <QMetaType>
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
    if (!qss.isEmpty())
        app.setStyleSheet(qss);
}

static void applyAppFont(QApplication &app)
{
    const QStringList uiCandidates = {
        QStringLiteral("Segoe UI"),
        QStringLiteral("Microsoft YaHei UI"),
        QStringLiteral("Microsoft YaHei"),
        QStringLiteral("PingFang SC"),
    };
    QString family = QStringLiteral("Microsoft YaHei");
    const QStringList available = QFontDatabase::families();
    for (const QString &name : uiCandidates) {
        if (available.contains(name)) {
            family = name;
            break;
        }
    }
    QFont font(family);
    font.setPointSize(10);
    app.setFont(font);
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

static User resolveStartupUser(int argc, char *argv[], LoginDialog **keepAlive)
{
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QLatin1String("--auto-login")) {
            UserDAO dao;
            const QList<User> users = dao.findAllUsers();
            if (!users.isEmpty())
                return users.first();
            return User{};
        }
    }

    // 登录框保持到主循环结束再销毁，避免关闭瞬间与主窗口初始化抢事件
    auto *loginDialog = new LoginDialog(nullptr);
    *keepAlive = loginDialog;
    if (loginDialog->exec() != QDialog::Accepted)
        return User{};
    return loginDialog->user();
}

int main(int argc, char *argv[])
{
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

    LoginDialog *loginKeepAlive = nullptr;
    const User user = resolveStartupUser(argc, argv, &loginKeepAlive);
    startupLog(user.id > 0 ? "login_ok" : "login_cancel");
    if (user.id <= 0) {
        delete loginKeepAlive;
        return 0;
    }

    if (loginKeepAlive) {
        loginKeepAlive->hide();
    }

    startupLog("mainwindow_ctor");
    MainWindow window;
    window.setWindowTitle(QStringLiteral("膳衡 · 智能营养膳食推荐系统"));
    window.resize(1180, 760);
    window.setMinimumSize(980, 680);

    startupLog("mainwindow_show");
    window.show();
    window.raise();
    window.activateWindow();

    QTimer::singleShot(0, &window, [&window, user, loginKeepAlive]() {
        startupLog("startWithUser");
        window.startWithUser(user);
        startupLog("startWithUser_done");
        if (loginKeepAlive)
            loginKeepAlive->deleteLater();
    });

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

    startupLog("exec");
    return app.exec();
}
