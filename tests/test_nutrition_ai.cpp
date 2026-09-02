#include "../src/services/NutritionAiService.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

#include <cstdio>

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
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);

    const FoodVisionResult food = NutritionAiService::parseFoodResult(QStringLiteral(
        "```json\n{\"food_name\":\"番茄鸡蛋饭\",\"serving_grams\":420,"
        "\"calories\":568,\"protein\":23.4,\"carbs\":72.1,\"fat\":18.5,"
        "\"confidence\":0.78,\"summary\":\"按一盘估算\","
        "\"assumptions\":[\"米饭约250克\",\"含少量烹调油\"]}\n```"),
        QStringLiteral("test"));
    expect(food.ok && food.foodName == QStringLiteral("番茄鸡蛋饭"),
           "多模态食物识别 JSON 可解析");
    expect(qAbs(food.servingGrams - 420.0) < 0.01
               && qAbs(food.calories - 568.0) < 0.01 && food.items.size() == 1
               && food.assumptions.size() == 2,
           "食物份量、总营养和估算依据完整保留");

    const FoodVisionResult calibrated = NutritionAiService::parseFoodResult(QStringLiteral(
        "{\"items\":[{\"food_name\":\"醋溜茄子\",\"item_type\":\"菜品\","
        "\"portion_min_grams\":220,\"portion_max_grams\":280,\"serving_grams\":250,"
        "\"calibration_basis\":\"俯视图+侧视图+23厘米餐盘\",\"calories\":286,"
        "\"protein\":4.2,\"carbs\":31,\"fat\":17,\"confidence\":0.86}]}"));
    expect(calibrated.ok && calibrated.items.size() == 1
               && qAbs(calibrated.servingMinGrams - 220.0) < 0.01
               && qAbs(calibrated.servingMaxGrams - 280.0) < 0.01
               && calibrated.calibrationBasis.contains(QStringLiteral("侧视图")),
           "双照片校准结果保留份量区间、推荐值和参照依据");

    const FoodVisionResult encyclopedia = NutritionAiService::parseFoodResult(QStringLiteral(
        "{\"food_name\":\"西兰花\",\"item_type\":\"食材\",\"category\":\"蔬菜\","
        "\"serving_grams\":300,\"calories\":102,\"protein\":8.4,\"carbs\":19.8,"
        "\"fat\":1.2,\"confidence\":0.86,\"taste\":\"清甜脆嫩\","
        "\"common_uses\":\"清炒、焯拌\",\"nutrition_highlights\":\"富含膳食纤维\","
        "\"summary\":\"按一颗中等大小估算\","
        "\"answer\":\"适合清炒或搭配鸡胸肉\",\"assumptions\":[]}"));
    expect(encyclopedia.ok && encyclopedia.itemType == QStringLiteral("食材")
               && encyclopedia.taste.contains(QStringLiteral("清甜"))
               && encyclopedia.commonUses.contains(QStringLiteral("清炒"))
               && encyclopedia.answer.contains(QStringLiteral("鸡胸肉")),
           "图片百科字段可解析并用于推荐页回答");

    const FoodVisionResult multiple = NutritionAiService::parseFoodResult(QStringLiteral(
        "{\"items\":["
        "{\"food_name\":\"三文鱼\",\"item_type\":\"食材\",\"serving_grams\":250,"
        "\"calories\":520,\"protein\":50,\"carbs\":0,\"fat\":32,\"confidence\":0.91},"
        "{\"food_name\":\"鸡蛋\",\"item_type\":\"食材\",\"serving_grams\":150,"
        "\"calories\":216,\"protein\":19,\"carbs\":2,\"fat\":15,\"confidence\":0.88}],"
        "\"summary\":\"识别到两种食材\"}"));
    expect(multiple.ok && multiple.items.size() == 2
               && multiple.foodName == QStringLiteral("三文鱼、鸡蛋"),
           "同一照片中的多种菜品或食材可逐项解析");
    expect(qAbs(multiple.servingGrams - 400.0) < 0.01
               && qAbs(multiple.calories - 736.0) < 0.01,
           "多目标识别同时提供聚合重量与营养总量");

    const FoodVisionResult cookedDish = NutritionAiService::parseFoodResult(QStringLiteral(
        "{\"items\":[{\"food_name\":\"醋溜茄子\",\"item_type\":\"菜品\","
        "\"category\":\"素菜\",\"serving_grams\":250,\"calories\":286,"
        "\"protein\":4.2,\"carbs\":31.0,\"fat\":17.0,\"confidence\":0.85}]}") );
    expect(cookedDish.ok && cookedDish.items.size() == 1
               && cookedDish.foodName == QStringLiteral("醋溜茄子")
               && cookedDish.itemType == QStringLiteral("菜品"),
           "单盘成菜按最终菜名保留为一个识别项目");

    const FoodVisionResult truncated = NutritionAiService::parseFoodResult(QStringLiteral(
        "{\"items\":["
        "{\"food_name\":\"三文鱼\",\"item_type\":\"食材\",\"serving_grams\":250,"
        "\"calories\":520,\"protein\":50,\"carbs\":0,\"fat\":32,\"confidence\":0.91},"
        "{\"food_name\":\"鸡蛋\",\"item_type\":\"食材\",\"serving_grams\":150,"
        "\"calories\":216,\"protein\":19,\"carbs\":2,\"fat\":15,\"confidence\":0.88},"
        "{\"food_name\":\"菠菜\",\"serving_grams\":200"));
    expect(truncated.ok && truncated.items.size() == 2
               && truncated.foodName == QStringLiteral("三文鱼、鸡蛋"),
           "截断的多食材 JSON 可恢复其中完整识别项");

    const FoodVisionResult invalidFood = NutritionAiService::parseFoodResult(
        QStringLiteral("{\"food_name\":\"米饭\",\"calories\":120}"));
    expect(!invalidFood.ok && !invalidFood.error.isEmpty(),
           "缺少份量的识别结果不会进入保存流程");

    const RecipeDnaResult dna = NutritionAiService::parseRecipeDnaResult(QStringLiteral(
        "{\"name\":\"轻脂红烧肉\",\"category\":\"晚餐\",\"dish_role\":\"meat\","
        "\"cook_minutes\":55,\"ingredients\":["
        "{\"name\":\"猪五花肉\",\"grams\":140,\"display\":\"约140g\"},"
        "{\"name\":\"杏鲍菇\",\"grams\":80,\"display\":\"半根约80g\"},"
        "{\"name\":\"肥瘦猪肉馅\",\"grams\":52.3,\"display\":\"42.3 g\"}],"
        "\"steps\":\"1. 焯水。\\n2. 少油煸香后炖煮。\","
        "\"estimated_nutrition\":{\"calories\":420,\"protein\":25,"
        "\"carbs\":12,\"fat\":28},\"change_summary\":\"减肉并用菌菇补充口感。\"}"),
        QStringLiteral("test"));
    expect(dna.ok && dna.ingredients.size() == 3 && dna.steps.contains(QStringLiteral("炖煮")),
           "食谱DNA结果保留完整主料与可执行步骤");
    expect(dna.fat == 28.0 && dna.changeSummary.contains(QStringLiteral("菌菇")),
           "DNA改造营养估算和风味说明可解析");
    expect(dna.ingredients.at(2).quantity == 52.3
               && dna.ingredients.at(2).quantityText == QStringLiteral("52.3g"),
           "DNA原料克数与自然单位中的约克数不一致时自动统一");

    Recipe original;
    original.id = 10;
    original.name = QStringLiteral("三色糕");
    original.totalProtein = 20.0;
    original.totalFat = 10.0;
    original.ingredients = {
        {QStringLiteral("芸豆面"), 80.0, QStringLiteral("80g")},
        {QStringLiteral("豆沙馅"), 30.0, QStringLiteral("30g")},
        {QStringLiteral("白糖"), 10.0, QStringLiteral("10g")},
    };
    RecipeDnaResult unchanged;
    unchanged.ok = true;
    unchanged.name = QStringLiteral("甜香三色糕");
    unchanged.steps = QStringLiteral("1. 混合。2. 蒸熟。");
    unchanged.calories = 450;
    unchanged.protein = 20;
    unchanged.carbs = 90;
    unchanged.fat = 10;
    unchanged.ingredients = {
        {QStringLiteral("芸豆面"), 80.0, QStringLiteral("80g")},
        {QStringLiteral("豆沙馅"), 30.0, QStringLiteral("30g")},
        {QStringLiteral("白糖"), 10.0, QStringLiteral("10g")},
    };
    expect(!NutritionAiService::validateRecipeDnaChange(
                original, unchanged, QStringLiteral("提高甜度")).isEmpty(),
           "提高甜度但原料克数不变会被拒绝");

    RecipeDnaResult sweeter = unchanged;
    sweeter.ingredients[2].quantity = 16.0;
    expect(NutritionAiService::validateRecipeDnaChange(
               original, sweeter, QStringLiteral("提高甜度")).isEmpty(),
           "增加甜味原料后可通过甜度改造校验");

    expect(!NutritionAiService::validateRecipeDnaChange(
                original, unchanged, QStringLiteral("提高蛋白质")).isEmpty(),
           "提高蛋白质但配方和蛋白质数值不变会被拒绝");
    RecipeDnaResult highProtein = unchanged;
    highProtein.ingredients.append(
        {QStringLiteral("脱脂奶粉"), 20.0, QStringLiteral("2勺约20g")});
    highProtein.protein = 24.0;
    expect(NutritionAiService::validateRecipeDnaChange(
               original, highProtein, QStringLiteral("提高蛋白质")).isEmpty(),
           "新增蛋白质原料且营养提高后可通过校验");

    if (argc > 1) {
        NutritionAiService service;
        int liveExitCode = 3;
        const QString liveMode = QString::fromLocal8Bit(argv[1]);
        const bool dnaLive = liveMode.startsWith(QLatin1String("--dna"));
        if (dnaLive) {
            QObject::connect(&service, &NutritionAiService::recipeTransformFinished,
                             &application, [&](const RecipeDnaResult &result) {
                if (result.ok) {
                    std::fprintf(stdout,
                                 "DNA_LIVE_OK provider=%s ingredients=%lld kcal=%.1f fat=%.1f\n",
                                 qPrintable(result.provider),
                                 static_cast<long long>(result.ingredients.size()),
                                 result.calories, result.fat);
                    liveExitCode = 0;
                } else {
                    std::fprintf(stderr, "DNA_LIVE_FAIL %s\n", qPrintable(result.error));
                    liveExitCode = 4;
                }
                application.quit();
            });
        } else {
            QObject::connect(&service, &NutritionAiService::foodAnalysisFinished,
                             &application, [&](const FoodVisionResult &result) {
                if (result.ok) {
                    std::fprintf(stdout, "LIVE_OK provider=%s name=%s grams=%.1f kcal=%.1f confidence=%.2f\n",
                                 qPrintable(result.provider), qPrintable(result.foodName),
                                 result.servingGrams, result.calories, result.confidence);
                    liveExitCode = 0;
                } else {
                    std::fprintf(stderr, "LIVE_FAIL %s\n", qPrintable(result.error));
                    liveExitCode = 4;
                }
                application.quit();
            });
        }
        QTimer::singleShot(75000, &application, [&]() {
            std::fprintf(stderr, "LIVE_FAIL timeout\n");
            liveExitCode = 5;
            service.cancel();
            application.quit();
        });
        if (dnaLive) {
            Recipe source;
            source.id = 1;
            source.name = QStringLiteral("红烧肉");
            source.category = QStringLiteral("晚餐");
            source.dishRole = QStringLiteral("meat");
            source.cookMinutes = 60;
            source.totalCalories = 650;
            source.totalProtein = 27;
            source.totalCarbs = 18;
            source.totalFat = 52;
            source.ingredients = {
                {QStringLiteral("猪五花肉"), 220.0, QStringLiteral("220g")},
                {QStringLiteral("冰糖"), 15.0, QStringLiteral("15g")},
                {QStringLiteral("姜"), 8.0, QStringLiteral("4片约8g")},
            };
            source.steps = QStringLiteral("1. 五花肉焯水。\n2. 炒糖色后加入肉块。\n3. 加水小火炖至软烂。");
            QString instruction = QStringLiteral("把脂肪减少30%，保留红烧风味和软糯口感");
            if (liveMode == QLatin1String("--dna-sweet"))
                instruction = QStringLiteral("提高甜度，但仍要保持甜而不腻");
            else if (liveMode == QLatin1String("--dna-protein"))
                instruction = QStringLiteral("提高蛋白质，热量不要明显上升");
            service.transformRecipe(source, instruction);
        } else {
            service.analyzeFoodImage(QString::fromLocal8Bit(argv[1]));
        }
        application.exec();
        return liveExitCode;
    }

    return failures == 0 ? 0 : 1;
}
