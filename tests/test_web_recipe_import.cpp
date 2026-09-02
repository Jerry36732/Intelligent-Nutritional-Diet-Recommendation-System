#include "../src/services/WebRecipeImportService.h"

#include <QCoreApplication>
#include <QDebug>

namespace {
int failures = 0;

void expect(bool condition, const char *message)
{
    if (condition)
        qInfo() << "OK  :" << message;
    else {
        qCritical() << "FAIL:" << message;
        ++failures;
    }
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const QByteArray recipeHtml = R"HTML(
        <html><head><script type="application/ld+json">
        {"@context":"https://schema.org","@type":"Recipe","name":"番茄炒蛋",
         "recipeIngredient":["番茄 300g","鸡蛋 2个"],
         "recipeInstructions":[{"@type":"HowToStep","text":"番茄切块。"},
                                {"@type":"HowToStep","text":"鸡蛋炒熟后加入番茄。"}],
         "totalTime":"PT18M","recipeCategory":"素菜"}
        </script></head><body>请登录后收藏本菜谱</body></html>
    )HTML";
    const WebRecipeImportResult recipe = WebRecipeImportService::parseHtml(
        recipeHtml, QStringLiteral("https://example.com/recipe/1"), 200, QStringLiteral("text/html; charset=utf-8"));
    expect(recipe.isComplete() && recipe.name == QStringLiteral("番茄炒蛋")
               && recipe.ingredients.size() == 2 && recipe.steps.size() == 2
               && recipe.minutes == 18 && recipe.category == QStringLiteral("素菜"),
           "完整 Schema.org Recipe 优先于页面中的登录提示并正确解析");

    const QByteArray loginHtml = u8R"HTML(
        <html><head><title>用户登录 - 下厨房</title></head>
        <body><h1>账号登录</h1><p>请先登录后继续</p></body></html>
    )HTML";
    const WebRecipeImportResult login = WebRecipeImportService::parseHtml(
        loginHtml, QStringLiteral("https://www.xiachufang.com/auth/login/"));
    expect(login.state == WebRecipeImportResult::State::LoginRequired
               && login.name.isEmpty() && login.message.contains(QStringLiteral("浏览器登录")),
           "登录页不会再被标题误识别成食谱");

    const QByteArray captchaHtml = u8R"HTML(
        <html><head><title>安全验证</title></head>
        <body><div class="geetest">请拖动滑块完成验证</div></body></html>
    )HTML";
    const WebRecipeImportResult captcha = WebRecipeImportService::parseHtml(
        captchaHtml, QStringLiteral("https://example.com/verify"));
    expect(captcha.state == WebRecipeImportResult::State::VerificationRequired
               && captcha.message.contains(QStringLiteral("不会绕过")),
           "滑块或验证码页面会进入合规的复制正文降级流程");

    const WebRecipeImportResult blocked = WebRecipeImportService::parseHtml(
        QByteArray("<html><title>Forbidden</title><body>Access Denied</body></html>"),
        QStringLiteral("https://example.com/recipe"), 403, QStringLiteral("text/html"));
    expect(blocked.state == WebRecipeImportResult::State::AccessBlocked,
           "HTTP 403 或访问拒绝页面会被识别为受限网页");

    const QString copied = QStringLiteral(
        "下厨房\n"
        "宫保鸡丁\n"
        "用料\n"
        "鸡胸肉\n"
        "300g\n"
        "花生\n"
        "50g\n"
        "干辣椒 8g\n"
        "做法\n"
        "1. 鸡胸肉切丁并腌制。\n"
        "2. 热锅炒香配料，放入鸡丁和花生翻炒。\n");
    const WebRecipeImportResult pasted = WebRecipeImportService::parseCopiedText(copied);
    expect(pasted.isComplete() && pasted.name == QStringLiteral("宫保鸡丁")
               && pasted.ingredients.size() == 3 && pasted.steps.size() == 2
               && pasted.ingredients.first() == QStringLiteral("鸡胸肉 300g")
               && pasted.steps.first().startsWith(QStringLiteral("1. ")),
           "下厨房复制正文中的站点导航与分列用量可被正确处理");

    const QString detailedSteps = QStringLiteral(
        "红烧肉\n"
        "原料清单\n"
        "✔ 五花肉 500克\n"
        "✔ 生抽 3勺\n"
        "✔ 老抽 1勺\n"
        "✔ 热水适量\n"
        "详细制作步骤（附关键技巧）\n"
        "第一步：处理猪肉\n"
        "将五花肉切成 3cm 见方的块状，冷水下锅焯水。\n"
        "第二步：炒糖色\n"
        "放入冰糖，小火炒至琥珀色。\n");
    const WebRecipeImportResult detailed =
        WebRecipeImportService::parseCopiedText(detailedSteps);
    expect(detailed.isComplete() && detailed.name == QStringLiteral("红烧肉")
               && detailed.ingredients.size() == 4 && detailed.steps.size() == 2
               && !detailed.ingredients.join(QLatin1Char('\n')).contains(QStringLiteral("制作步骤"))
               && !detailed.ingredients.join(QLatin1Char('\n')).contains(QStringLiteral("处理猪肉"))
               && detailed.steps.first().contains(QStringLiteral("处理猪肉"))
               && detailed.steps.first().contains(QStringLiteral("3cm")),
           "扩展步骤标题和中文序号会终止原料段，并合并步骤标题与说明");

    const QByteArray verifiedPageHtml = u8R"HTML(
        <html><head><title>红烧肉 - 家常菜谱</title></head><body>
        <h1>红烧肉</h1><h2>原料清单</h2>
        <p>五花肉 500克</p><p>生抽 3勺</p><p>热水适量</p>
        <h2>详细制作步骤（附关键技巧）</h2>
        <h3>第一步：处理猪肉</h3><p>将五花肉切成 3cm 见方的块状。</p>
        <h3>第二步：炒糖色</h3><p>放入冰糖，小火炒至琥珀色。</p>
        </body></html>
    )HTML";
    const WebRecipeImportResult verifiedPage = WebRecipeImportService::parseHtml(
        verifiedPageHtml, QStringLiteral("https://example.com/verified-recipe"), 200,
        QStringLiteral("text/html; charset=utf-8"));
    expect(verifiedPage.isComplete() && verifiedPage.ingredients.size() == 3
               && verifiedPage.steps.size() == 2
               && !verifiedPage.ingredients.join(QLatin1Char('\n')).contains(QStringLiteral("制作步骤"))
               && verifiedPage.steps.first().contains(QStringLiteral("处理猪肉")),
           "完成网页验证后可从无 Schema 的可见正文提取食谱并正确分离原料与步骤");

    const WebRecipeImportResult incomplete = WebRecipeImportService::parseCopiedText(
        QStringLiteral("只有一个菜名"));
    expect(incomplete.state == WebRecipeImportResult::State::Incomplete
               && incomplete.message.contains(QStringLiteral("原料"))
               && incomplete.message.contains(QStringLiteral("步骤")),
           "复制正文不完整时明确指出缺失字段");

    return failures == 0 ? 0 : 1;
}
