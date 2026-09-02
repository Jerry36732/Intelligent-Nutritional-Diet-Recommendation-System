#include "../src/ui/RecipeEditorDialog.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QEventLoop>
#include <QFile>
#include <QFrame>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QWebEngineView>

#include <cstdio>

namespace {
QPushButton *buttonWithText(QWidget *root, const QString &text)
{
    for (QPushButton *button : root->findChildren<QPushButton *>()) {
        if (button->text() == text)
            return button;
    }
    return nullptr;
}
} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QFile style(QCoreApplication::applicationDirPath() + QStringLiteral("/styles.qss"));
    if (style.open(QIODevice::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(style.readAll()));

    const bool selfTest = QCoreApplication::arguments().contains(QStringLiteral("--self-test"));
    if (selfTest) {
        std::fprintf(stderr, "INFO: app-created self-test=true\n");
        std::fflush(stderr);
    }
    RecipeEditorDialog dialog(0, RecipeEditorDialog::Mode::WebImport);
    if (selfTest)
        dialog.setModal(false);
    if (selfTest) {
        std::fprintf(stderr, "INFO: dialog-constructed\n");
        std::fflush(stderr);
    }
    dialog.show();
    if (selfTest) {
        std::fprintf(stderr, "INFO: dialog-shown\n");
        std::fflush(stderr);
    }

    if (!selfTest)
        return app.exec();

    QTimer::singleShot(0, &dialog, [&dialog]() {
        std::fprintf(stderr, "INFO: self-test-started\n");
        std::fflush(stderr);
        int failures = 0;
        auto check = [&failures](bool condition, const char *message) {
            std::fprintf(stderr, "%s: %s\n", condition ? "OK" : "FAIL", message);
            if (!condition)
                ++failures;
        };

        QPushButton *toggle = buttonWithText(&dialog, QStringLiteral("页面需要登录或验证？改用复制正文"));
        check(toggle != nullptr, "存在登录/滑块降级入口");
        if (toggle)
            toggle->click();
        QApplication::processEvents();

        QFrame *panel = dialog.findChild<QFrame *>(QStringLiteral("WebImportFallback"));
        QPlainTextEdit *copied = dialog.findChild<QPlainTextEdit *>(QStringLiteral("WebRecipeCopiedText"));
        QPushButton *verifiedPage =
            dialog.findChild<QPushButton *>(QStringLiteral("WebRecipeOpenVerifiedPage"));
        check(panel && panel->isVisible(), "复制正文面板可以展开");
        check(verifiedPage && verifiedPage->text() == QStringLiteral("打开网页并完成验证"),
              "登录或滑块页面可进入应用内验证与提取流程");
        check(dialog.height() >= 820 && panel && dialog.rect().contains(panel->geometry()),
              "展开后窗体增高且面板没有被裁切");
        QPlainTextEdit *ingredients =
            dialog.findChild<QPlainTextEdit *>(QStringLiteral("WebRecipeIngredients"));
        QPlainTextEdit *steps = dialog.findChild<QPlainTextEdit *>(QStringLiteral("WebRecipeSteps"));
        if (ingredients && steps)
            std::fprintf(stderr, "INFO: ingredient-height=%d step-height=%d\n",
                         ingredients->height(), steps->height());
        check(ingredients && ingredients->height() >= 130 && steps && steps->height() >= 130,
              "展开降级区后原料和步骤输入框仍保持可读高度");

        if (copied) {
            copied->setPlainText(QStringLiteral(
                "宫保鸡丁\n用料\n鸡胸肉 300g\n花生 50g\n干辣椒 8g\n"
                "做法\n1. 鸡胸肉切丁并腌制。\n2. 放入鸡丁和花生翻炒。"));
        }
        QPushButton *parse = buttonWithText(&dialog, QStringLiteral("解析粘贴内容"));
        check(parse != nullptr, "存在解析粘贴内容按钮");
        if (parse)
            parse->click();
        QApplication::processEvents();

        QLineEdit *name = dialog.findChild<QLineEdit *>(QStringLiteral("WebRecipeName"));
        check(name && name->text() == QStringLiteral("宫保鸡丁"), "复制正文可回填菜名");
        check(ingredients && ingredients->toPlainText().contains(QStringLiteral("鸡胸肉 300g"))
                  && ingredients->toPlainText().contains(QStringLiteral("花生 50g")),
              "复制正文可回填完整原料");
        check(steps && steps->toPlainText().contains(QStringLiteral("1. 鸡胸肉切丁并腌制。"))
                  && steps->toPlainText().contains(QStringLiteral("2. 放入鸡丁和花生翻炒。")),
              "复制正文可回填并编号制作步骤");

        if (copied) {
            copied->setPlainText(QStringLiteral(
                "红烧肉\n原料清单\n✔ 五花肉 500克\n✔ 生抽 3勺\n✔ 老抽 1勺\n"
                "✔ 热水适量\n详细制作步骤（附关键技巧）\n第一步：处理猪肉\n"
                "将五花肉切成 3cm 见方的块状，冷水下锅焯水。"));
        }
        if (parse)
            parse->click();
        QApplication::processEvents();
        check(ingredients && !ingredients->toPlainText().contains(QStringLiteral("制作步骤"))
                  && !ingredients->toPlainText().contains(QStringLiteral("处理猪肉")),
              "扩展步骤标题之后的内容不会进入原料输入框");
        check(steps && steps->toPlainText().contains(QStringLiteral("处理猪肉"))
                  && steps->toPlainText().contains(QStringLiteral("3cm")),
              "中文序号步骤及其说明会进入制作步骤输入框");

        QLineEdit *url = dialog.findChild<QLineEdit *>(QStringLiteral("WebRecipeUrl"));
        if (url)
            url->setText(QStringLiteral("http://127.0.0.1/verified-recipe"));
        if (verifiedPage)
            verifiedPage->click();
        QApplication::processEvents();

        QPointer<QDialog> browser = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QWebEngineView *webView = browser
                                      ? browser->findChild<QWebEngineView *>(
                                            QStringLiteral("WebRecipeVerificationView"))
                                      : nullptr;
        QPushButton *extract = browser
                                   ? browser->findChild<QPushButton *>(
                                         QStringLiteral("WebRecipeExtractVerifiedPage"))
                                   : nullptr;
        check(browser && webView && extract, "应用内网页验证窗体和提取按钮可正常打开");

        if (webView && extract) {
            const QString html = QStringLiteral(
                "<html><head><title>红烧肉 - 测试菜谱</title></head><body>"
                "<h1>红烧肉</h1><h2>原料清单</h2>"
                "<p>五花肉 500克</p><p>生抽 3勺</p><p>热水适量</p>"
                "<h2>详细制作步骤（附关键技巧）</h2>"
                "<h3>第一步：处理猪肉</h3><p>将五花肉切成 3cm 见方的块状。</p>"
                "<h3>第二步：炒糖色</h3><p>放入冰糖，小火炒至琥珀色。</p>"
                "</body></html>");
            QEventLoop loadLoop;
            bool loaded = false;
            QObject::connect(webView, &QWebEngineView::loadFinished, &loadLoop,
                             [&loadLoop, &loaded](bool ok) {
                loaded = ok;
                loadLoop.quit();
            });
            QTimer::singleShot(5000, &loadLoop, &QEventLoop::quit);
            webView->setHtml(html, QUrl(QStringLiteral("http://127.0.0.1/verified-recipe")));
            loadLoop.exec();
            check(loaded, "应用内网页可在验证后继续读取当前页面");

            extract->click();
            QEventLoop extractLoop;
            QTimer::singleShot(1200, &extractLoop, &QEventLoop::quit);
            extractLoop.exec();
            check(name && name->text() == QStringLiteral("红烧肉"),
                  "验证后的网页可直接回填菜品名称");
            check(ingredients && ingredients->toPlainText().contains(QStringLiteral("五花肉 500克"))
                      && !ingredients->toPlainText().contains(QStringLiteral("制作步骤"))
                      && !ingredients->toPlainText().contains(QStringLiteral("处理猪肉")),
                  "验证后的网页回填原料时不会混入制作步骤");
            check(steps && steps->toPlainText().contains(QStringLiteral("处理猪肉"))
                      && steps->toPlainText().contains(QStringLiteral("炒糖色")),
                  "验证后的网页可回填完整制作步骤");
        }
        if (browser)
            browser->reject();

        QCoreApplication::exit(failures == 0 ? 0 : 1);
    });
    return app.exec();
}
