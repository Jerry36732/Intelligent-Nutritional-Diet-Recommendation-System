#include "NutritionAiService.h"
#include "FlavorFingerprintService.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QtGlobal>

#include <utility>

namespace {
QJsonObject readAiConfig()
{
    const QStringList paths = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/data/ai_config.json"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/ai_config.json"),
        QStringLiteral("data/ai_config.json"),
    };
    for (const QString &path : paths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        if (document.isObject())
            return document.object();
    }
    return {};
}

QString configuredString(const QString &key, const QString &fallback = {})
{
    const QString value = readAiConfig().value(key).toString().trimmed();
    return value.isEmpty() ? fallback : value;
}

QString apiKey()
{
    const QString configured = configuredString(QStringLiteral("siliconflow_api_key"));
    return configured.isEmpty() ? qEnvironmentVariable("SILICONFLOW_API_KEY").trimmed()
                                : configured;
}

QString jsonObjectText(QString content)
{
    content = content.trimmed();
    content.remove(QStringLiteral("```json"), Qt::CaseInsensitive);
    content.remove(QStringLiteral("```"));
    const qsizetype first = content.indexOf(QLatin1Char('{'));
    const qsizetype last = content.lastIndexOf(QLatin1Char('}'));
    if (first >= 0 && last > first)
        return content.mid(first, last - first + 1);
    return content;
}

QJsonArray completeItemsFromTruncatedJson(QString content)
{
    content.remove(QStringLiteral("```json"), Qt::CaseInsensitive);
    content.remove(QStringLiteral("```"));
    const qsizetype itemsKey = content.indexOf(QStringLiteral("\"items\""));
    const qsizetype arrayStart = itemsKey >= 0
        ? content.indexOf(QLatin1Char('['), itemsKey) : -1;
    if (arrayStart < 0)
        return {};

    QJsonArray recovered;
    bool inString = false;
    bool escaped = false;
    int objectDepth = 0;
    qsizetype objectStart = -1;
    for (qsizetype i = arrayStart + 1; i < content.size(); ++i) {
        const QChar ch = content.at(i);
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == QLatin1Char('\\')) {
                escaped = true;
            } else if (ch == QLatin1Char('"')) {
                inString = false;
            }
            continue;
        }
        if (ch == QLatin1Char('"')) {
            inString = true;
            continue;
        }
        if (ch == QLatin1Char('{')) {
            if (objectDepth++ == 0)
                objectStart = i;
        } else if (ch == QLatin1Char('}') && objectDepth > 0) {
            if (--objectDepth == 0 && objectStart >= 0) {
                QJsonParseError itemError;
                const QJsonDocument itemDocument = QJsonDocument::fromJson(
                    content.mid(objectStart, i - objectStart + 1).toUtf8(), &itemError);
                if (itemDocument.isObject() && itemError.error == QJsonParseError::NoError)
                    recovered.append(itemDocument.object());
                objectStart = -1;
            }
        } else if (ch == QLatin1Char(']') && objectDepth == 0) {
            break;
        }
    }
    return recovered;
}

double number(const QJsonObject &object, const QString &key)
{
    const QJsonValue value = object.value(key);
    if (value.isDouble())
        return value.toDouble();
    return value.toString().toDouble();
}

QString normalizedCategory(const QString &value, const QString &fallback)
{
    if (value == QStringLiteral("早餐") || value == QStringLiteral("午餐")
        || value == QStringLiteral("晚餐"))
        return value;
    return fallback == QStringLiteral("早餐") || fallback == QStringLiteral("晚餐")
        ? fallback : QStringLiteral("午餐");
}

QString normalizedRole(const QString &value, const QString &fallback)
{
    const QStringList allowed = {QStringLiteral("breakfast"), QStringLiteral("staple"),
                                 QStringLiteral("meat"), QStringLiteral("vegetable"),
                                 QStringLiteral("soup"), QStringLiteral("dessert"),
                                 QStringLiteral("snack"), QStringLiteral("mixed"),
                                 QStringLiteral("drink")};
    if (allowed.contains(value))
        return value;
    return allowed.contains(fallback) ? fallback : QStringLiteral("mixed");
}

QJsonObject extractChatRoot(const QByteArray &raw, const QString &provider, QString *content,
                            QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(raw, &parseError);
    if (!document.isObject()) {
        *error = QStringLiteral("AI 返回格式无效：%1").arg(parseError.errorString());
        return {};
    }
    const QJsonObject root = document.object();
    if (provider == QLatin1String("siliconflow")) {
        const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
        if (!choices.isEmpty())
            *content = choices.first().toObject().value(QStringLiteral("message"))
                           .toObject().value(QStringLiteral("content")).toString();
        if (content->isEmpty())
            *error = root.value(QStringLiteral("message")).toString(
                QStringLiteral("云端模型没有返回识别内容。"));
    } else {
        *content = root.value(QStringLiteral("message")).toObject()
                       .value(QStringLiteral("content")).toString();
        if (content->isEmpty())
            *error = root.value(QStringLiteral("error")).toString(
                QStringLiteral("本地模型没有返回识别内容。"));
    }
    return root;
}

QString responseErrorText(const QByteArray &raw, const QString &fallback)
{
    const QJsonObject root = QJsonDocument::fromJson(raw).object();
    QString message = root.value(QStringLiteral("message")).toString().trimmed();
    if (message.isEmpty()) {
        const QJsonValue errorValue = root.value(QStringLiteral("error"));
        message = errorValue.isObject()
            ? errorValue.toObject().value(QStringLiteral("message")).toString().trimmed()
            : errorValue.toString().trimmed();
    }
    return message.isEmpty() ? fallback : message;
}

QString ingredientKey(QString value)
{
    value = value.trimmed().toLower();
    value.remove(QLatin1Char(' '));
    value.remove(QStringLiteral("（"));
    value.remove(QStringLiteral("）"));
    value.remove(QLatin1Char('('));
    value.remove(QLatin1Char(')'));
    return value;
}

bool containsAny(const QString &text, const QStringList &needles)
{
    for (const QString &needle : needles) {
        if (text.contains(needle, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

QString formattedGrams(double grams)
{
    const int decimals = qAbs(grams - qRound(grams)) < 0.05 ? 0 : 1;
    return QString::number(grams, 'f', decimals);
}

QString consistentQuantityText(QString display, double grams)
{
    display = display.trimmed();
    const QString gramsText = formattedGrams(grams) + QStringLiteral("g");
    if (display.isEmpty())
        return gramsText;

    static const QRegularExpression gramPattern(
        QStringLiteral("(\\d+(?:\\.\\d+)?)\\s*(?:g|克)"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch lastMatch;
    QRegularExpressionMatchIterator matches = gramPattern.globalMatch(display);
    while (matches.hasNext())
        lastMatch = matches.next();
    if (!lastMatch.hasMatch())
        return display + (display.endsWith(QStringLiteral("约")) ? QString() : QStringLiteral("约"))
               + gramsText;

    const double displayedGrams = lastMatch.captured(1).toDouble();
    const double tolerance = qMax(0.15, grams * 0.005);
    if (qAbs(displayedGrams - grams) <= tolerance)
        return display;
    display.replace(lastMatch.capturedStart(0), lastMatch.capturedLength(0), gramsText);
    return display;
}

FoodVisionItem foodVisionItemFromJson(const QJsonObject &object)
{
    FoodVisionItem item;
    item.foodName = object.value(QStringLiteral("food_name")).toString().trimmed();
    item.itemType = object.value(QStringLiteral("item_type")).toString().trimmed();
    item.category = object.value(QStringLiteral("category")).toString().trimmed();
    item.taste = object.value(QStringLiteral("taste")).toString().trimmed();
    item.commonUses = object.value(QStringLiteral("common_uses")).toString().trimmed();
    item.nutritionHighlights = object.value(QStringLiteral("nutrition_highlights"))
                                   .toString().trimmed();
    item.servingGrams = number(object, QStringLiteral("serving_grams"));
    item.calories = number(object, QStringLiteral("calories"));
    item.protein = number(object, QStringLiteral("protein"));
    item.carbs = number(object, QStringLiteral("carbs"));
    item.fat = number(object, QStringLiteral("fat"));
    item.confidence = qBound(0.0, number(object, QStringLiteral("confidence")), 1.0);
    item.summary = object.value(QStringLiteral("summary")).toString().trimmed();
    for (const QJsonValue &value : object.value(QStringLiteral("assumptions")).toArray()) {
        const QString assumption = value.toString().trimmed();
        if (!assumption.isEmpty())
            item.assumptions.append(assumption);
    }
    item.servingMinGrams = number(object, QStringLiteral("portion_min_grams"));
    item.servingMaxGrams = number(object, QStringLiteral("portion_max_grams"));
    item.calibrationBasis = object.value(QStringLiteral("calibration_basis"))
                                .toString().trimmed();
    if (item.servingMinGrams <= 0.0)
        item.servingMinGrams = item.servingGrams * 0.80;
    if (item.servingMaxGrams <= 0.0)
        item.servingMaxGrams = item.servingGrams * 1.20;
    if (item.servingMinGrams > item.servingMaxGrams)
        std::swap(item.servingMinGrams, item.servingMaxGrams);
    item.servingMinGrams = qMin(item.servingMinGrams, item.servingGrams);
    item.servingMaxGrams = qMax(item.servingMaxGrams, item.servingGrams);
    if (item.calibrationBasis.isEmpty())
        item.calibrationBasis = QStringLiteral("单张图片与常见餐具尺寸估算");
    return item;
}

bool isValidFoodVisionItem(const FoodVisionItem &item)
{
    return !item.foodName.isEmpty() && item.servingGrams > 0.0 && item.calories >= 0.0
           && item.protein >= 0.0 && item.carbs >= 0.0 && item.fat >= 0.0;
}
} // namespace

NutritionAiService::NutritionAiService(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_timeout(new QTimer(this))
{
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(90000);
    connect(m_timeout, &QTimer::timeout, this, [this]() {
        if (m_reply)
            m_reply->abort();
    });
}

NutritionAiService::~NutritionAiService()
{
    // 析构期不能再让 QNetworkReply::finished 回调进入本对象。
    stopRequest(false);
}

bool NutritionAiService::isBusy() const
{
    return m_operation != Operation::None;
}

void NutritionAiService::analyzeFoodImage(const QString &imagePath)
{
    if (isBusy())
        cancel();
    m_calibrationContext.clear();
    analyzeImage(imagePath, Operation::FoodVision);
}

void NutritionAiService::analyzeFoodImages(const QStringList &imagePaths,
                                           const QString &calibrationContext)
{
    if (isBusy())
        cancel();
    m_calibrationContext = calibrationContext.trimmed();
    analyzeImages(imagePaths, Operation::FoodVision);
}

void NutritionAiService::analyzeIngredientImage(const QString &imagePath)
{
    analyzeImage(imagePath, Operation::IngredientVision);
}

void NutritionAiService::describeFoodImage(const QString &imagePath)
{
    analyzeImage(imagePath, Operation::FoodEncyclopedia);
}

void NutritionAiService::describeFoodImages(const QStringList &imagePaths,
                                            const QString &question)
{
    analyzeImages(imagePaths, Operation::FoodEncyclopedia, question);
}

void NutritionAiService::analyzeImage(const QString &imagePath, Operation operation)
{
    analyzeImages({imagePath}, operation);
}

void NutritionAiService::analyzeImages(const QStringList &imagePaths, Operation operation,
                                       const QString &question)
{
    if (isBusy())
        cancel();
    const QStringList paths = imagePaths.mid(0, 6);
    if (paths.isEmpty()) {
        FoodVisionResult result;
        result.error = QStringLiteral("请先选择至少一张食物或食材图片。");
        emit foodAnalysisFinished(result);
        return;
    }

    QList<QByteArray> encodedImages;
    for (const QString &path : paths) {
        QImage image(path);
        if (image.isNull()) {
            FoodVisionResult result;
            result.error = QStringLiteral("无法读取图片“%1”，请重新选择 JPG、PNG 或 WebP 文件。")
                               .arg(QFileInfo(path).fileName());
            emit foodAnalysisFinished(result);
            return;
        }
        if (image.width() > 1344 || image.height() > 1344)
            image = image.scaled(1344, 1344, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QByteArray jpeg;
        QBuffer buffer(&jpeg);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "JPEG", 84)) {
            FoodVisionResult result;
            result.error = QStringLiteral("图片“%1”预处理失败，请更换后重试。")
                               .arg(QFileInfo(path).fileName());
            emit foodAnalysisFinished(result);
            return;
        }
        encodedImages.append(jpeg.toBase64());
    }

    m_imageBase64List = encodedImages;
    m_userQuestion = question.trimmed();
    m_recipe = {};
    m_instruction.clear();
    m_operation = operation;
    m_triedOllama = false;
    m_cloudRetryCount = 0;
    m_cancelled = false;
    m_lastNetworkError.clear();
    emit busyChanged(true);
    startSiliconFlow();
}

void NutritionAiService::transformRecipe(const Recipe &recipe, const QString &instruction)
{
    if (isBusy())
        cancel();
    if (!recipe.isValid() || instruction.trimmed().isEmpty()) {
        RecipeDnaResult result;
        result.error = QStringLiteral("请选择有效食谱并填写改造目标。");
        emit recipeTransformFinished(result);
        return;
    }
    m_recipe = recipe;
    m_instruction = instruction.trimmed();
    m_imageBase64List.clear();
    m_userQuestion.clear();
    m_calibrationContext.clear();
    m_operation = Operation::RecipeDna;
    m_triedOllama = false;
    m_cloudRetryCount = 0;
    m_cancelled = false;
    m_lastNetworkError.clear();
    m_dnaValidationError.clear();
    m_dnaRepairAttempted = false;
    emit busyChanged(true);
    startSiliconFlow();
}

void NutritionAiService::cancel()
{
    stopRequest(true);
}

void NutritionAiService::stopRequest(bool notifyBusyChange)
{
    const bool wasBusy = isBusy();
    m_cancelled = true;
    if (m_timeout)
        m_timeout->stop();

    // abort() 可能同步发出 finished。先清空成员并断开连接，避免 handleReply
    // 在弹窗关闭/析构过程中重入，或对同一 reply 重复安排释放。
    QNetworkReply *reply = std::exchange(m_reply, nullptr);
    if (reply) {
        QObject::disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }

    m_operation = Operation::None;
    m_imageBase64List.clear();
    m_userQuestion.clear();
    m_calibrationContext.clear();
    if (notifyBusyChange && wasBusy)
        emit busyChanged(false);
}

QString NutritionAiService::foodPrompt() const
{
    return QStringLiteral(
        "你是膳衡的成菜图像营养估算器。先按独立餐盘、碗或容器分割菜品，再识别每道菜的最终菜名。"
        "一盘已经烹饪完成的菜只能输出一个成菜项目，严禁把其中可见的茄子、辣椒、葱等配料拆成多个项目。"
        "例如一盘醋溜茄子必须输出 food_name=醋溜茄子、item_type=菜品，而不是输出茄子等原料。"
        "只有不同餐盘或容器中的食物才分别输出；图片若确实是未烹饪的独立原料，才可标为食材。"
        "若份量无法精确判断，请结合常见餐具尺寸给出保守估算，并降低 confidence。"
        "同一次请求可能包含同一餐食的俯视图和侧视图；必须联合判断餐盘面积、食物高度和参照物，不能当成两餐重复计算。"
        "只输出合法 JSON，不要 Markdown："
        "{\"items\":[{\"food_name\":\"单项名称\",\"item_type\":\"菜品或食材\","
        "\"category\":\"类别\",\"portion_min_grams\":数字,\"portion_max_grams\":数字,"
        "\"serving_grams\":推荐值数字,\"calibration_basis\":\"使用的角度和参照物\",\"calories\":数字,"
        "\"protein\":数字,\"carbs\":数字,\"fat\":数字,\"confidence\":0到1,"
        "\"taste\":\"口感\",\"common_uses\":\"常见用途\",\"nutrition_highlights\":\"营养特点\","
        "\"summary\":\"单项估算说明\",\"assumptions\":[\"关键估算依据\"]}],"
        "\"summary\":\"整张图片80字内说明\"}。"
        "每个营养字段均为该单项在图片中估算份量对应的总量；不要把每100克数据误当总量。"
        "无法确认时仍给合理区间中值，但必须在 assumptions 说明。");
}

QString NutritionAiService::ingredientPrompt() const
{
    return QStringLiteral(
        "你是膳衡的冰箱食材识别助手。逐项识别图片中全部不同的生鲜或包装食材，最多返回10种，分别估算可食用净重。"
        "包装有净含量文字时优先读取；没有参照物时按常见单个/一把/一盒质量估算并降低 confidence。"
        "只输出合法 JSON，不要 Markdown："
        "{\"items\":[{\"food_name\":\"规范食材名\",\"item_type\":\"食材\",\"category\":\"蔬菜/水果/肉禽蛋/鱼虾海鲜/乳制品/谷物/其他\","
        "\"serving_grams\":数字,\"calories\":数字,\"protein\":数字,\"carbs\":数字,\"fat\":数字,"
        "\"confidence\":0到1,\"taste\":\"味道口感\",\"common_uses\":\"常见用途\","
        "\"nutrition_highlights\":\"主要营养\",\"summary\":\"重量估算说明\",\"assumptions\":[\"估算依据\"]}],"
        "\"summary\":\"整张图片说明\"}。"
        "营养数值为各食材估算重量对应的总量；食材名去掉小块、一根等数量词。不得只选择画面主体。"
        "summary 和 assumptions 必须简短，优先保证 items 数组完整闭合。"
    );
}

QString NutritionAiService::encyclopediaPrompt() const
{
    return QStringLiteral(
        "你是膳衡的食物图片百科助手。逐项判断图片中的对象是菜品还是食材，并分别说明名称、味道口感、常见用途和主要营养，"
        "估算各自可食部分重量及对应的总热量和三大营养素。只输出合法 JSON，不要 Markdown："
        "{\"items\":[{\"food_name\":\"名称\",\"item_type\":\"菜品或食材\",\"category\":\"所属类别\","
        "\"serving_grams\":数字,\"calories\":数字,\"protein\":数字,\"carbs\":数字,\"fat\":数字,"
        "\"confidence\":0到1,\"taste\":\"味道与口感\",\"common_uses\":\"常用于什么\","
        "\"nutrition_highlights\":\"主要营养特点\",\"summary\":\"识别与估算说明\","
        "\"assumptions\":[\"无法从图片确认的假设\"]}],\"summary\":\"整张图片说明\","
        "\"answer\":\"结合图片对象和用户问题的直接回答；没有附加问题时为空字符串\"}。"
        "不确定时使用‘可能是’，降低 confidence，不编造品牌、疗效或精确配方。"
    );
}

QString NutritionAiService::imagePrompt() const
{
    QString prompt;
    if (m_operation == Operation::IngredientVision)
        prompt = ingredientPrompt();
    else if (m_operation == Operation::FoodEncyclopedia)
        prompt = encyclopediaPrompt();
    else
        prompt = foodPrompt();
    if (!m_userQuestion.isEmpty()) {
        prompt += QStringLiteral("\n用户同时问：%1\n请在 answer 字段中直接回答，并只引用图片中识别到的对象。")
                      .arg(m_userQuestion);
    }
    if (m_operation == Operation::FoodVision) {
        prompt += QStringLiteral("\n本次共%1张同一餐食照片。校准信息：%2。"
                                 "请输出合理份量区间，推荐值必须位于区间内；多角度清晰且有参照物时可提高置信度。")
                      .arg(m_imageBase64List.size())
                      .arg(m_calibrationContext.isEmpty()
                               ? QStringLiteral("未提供尺寸参照物") : m_calibrationContext);
    }
    return prompt;
}

QString NutritionAiService::dnaPrompt() const
{
    QJsonArray ingredients;
    for (const RecipeIngredient &ingredient : m_recipe.ingredients) {
        ingredients.append(QJsonObject{{QStringLiteral("name"), ingredient.foodName},
                                       {QStringLiteral("grams"), ingredient.quantity},
                                       {QStringLiteral("display"), ingredient.quantityText}});
    }
    const QJsonObject original{
        {QStringLiteral("name"), m_recipe.name},
        {QStringLiteral("category"), m_recipe.category},
        {QStringLiteral("dish_role"), m_recipe.dishRole},
        {QStringLiteral("cook_minutes"), m_recipe.cookMinutes},
        {QStringLiteral("ingredients"), ingredients},
        {QStringLiteral("steps"), m_recipe.steps},
        {QStringLiteral("nutrition"), QJsonObject{
             {QStringLiteral("calories"), m_recipe.totalCalories},
             {QStringLiteral("protein"), m_recipe.totalProtein},
             {QStringLiteral("carbs"), m_recipe.totalCarbs},
             {QStringLiteral("fat"), m_recipe.totalFat}}},
    };
    const FlavorFingerprint originalFlavor = FlavorFingerprintService().forRecipe(m_recipe);
    QJsonObject flavorJson;
    const QStringList flavorLabels = FlavorFingerprint::labels();
    for (int i = 0; i < flavorLabels.size(); ++i)
        flavorJson.insert(flavorLabels.at(i), qRound(originalFlavor.value(i)));
    QString repairInstruction;
    if (m_dnaRepairAttempted && !m_dnaValidationError.isEmpty()) {
        repairInstruction = QStringLiteral(
            "\n上次输出被校验拒绝，原因：%1。必须重新设计，禁止再次返回与原食谱相同的原料和克数。"
            "在 change_summary 中逐项写明‘原来多少克→现在多少克’以及新增/替换原料。")
                                .arg(m_dnaValidationError);
    }
    return QStringLiteral(
        "你是膳衡的食谱DNA改造器。根据用户目标改造原食谱，必须保留主要风味轮廓，并给出可真正烹饪的完整主料、克数和步骤。"
        "微量盐、味精等可省略营养计算，但肉蛋奶、主食、蔬菜等主料不能漏。不得宣称医疗效果。"
        "改造结果必须与原配方产生可验证的原料或克数变化，不能只改名称、说明或步骤。"
        "若目标含‘提高甜度’，必须增加糖、蜂蜜、甜味水果等甜味原料的克数或新增甜味原料；"
        "若目标含‘提高蛋白质’，必须新增或增加蛋、奶、肉、鱼虾、豆制品等蛋白质原料，并使估算蛋白质至少提高8%；"
        "若目标为脂肪减少30%，必须减少高脂原料/油脂克数或用低脂原料替换，估算脂肪至少下降25%。"
        "用户目标：%1\n原食谱JSON：%2\n原版风味指纹（0到100）：%3%4\n"
        "只输出合法 JSON，不要 Markdown："
        "{\"name\":\"改造后名称\",\"category\":\"早餐/午餐/晚餐\","
        "\"dish_role\":\"breakfast/staple/meat/vegetable/soup/dessert/snack/mixed/drink\","
        "\"cook_minutes\":数字,\"ingredients\":[{\"name\":\"原料\",\"grams\":数字,"
        "\"display\":\"可选的自然单位，如1勺约10g；其中约克数必须与grams完全一致\"}],\"steps\":\"编号步骤\","
        "\"estimated_nutrition\":{\"calories\":数字,\"protein\":数字,\"carbs\":数字,\"fat\":数字},"
        "\"change_summary\":\"说明怎样达到目标并保持风味，120字内\"}。")
        .arg(m_instruction,
             QString::fromUtf8(QJsonDocument(original).toJson(QJsonDocument::Compact)),
             QString::fromUtf8(QJsonDocument(flavorJson).toJson(QJsonDocument::Compact)),
             repairInstruction);
}

void NutritionAiService::startSiliconFlow()
{
    const QString key = apiKey();
    if (key.isEmpty()) {
        startOllama();
        return;
    }
    m_lastProvider = QStringLiteral("siliconflow");
    const bool visionOperation = m_operation != Operation::RecipeDna;
    const QString model = visionOperation
        ? configuredString(QStringLiteral("siliconflow_vision_model"),
                           QStringLiteral("Qwen/Qwen3-VL-8B-Instruct"))
        : configuredString(QStringLiteral("siliconflow_recipe_model"),
                           configuredString(QStringLiteral("siliconflow_model"),
                                            QStringLiteral("Qwen/Qwen3-8B")));
    const int outputTokens = m_operation == Operation::IngredientVision ? 2400
                           : visionOperation ? 1900 : 1400;
    QJsonObject body{{QStringLiteral("model"), model},
                     {QStringLiteral("temperature"), 0.15},
                     {QStringLiteral("max_tokens"), outputTokens},
                     {QStringLiteral("stream"), false},
                     {QStringLiteral("response_format"),
                      QJsonObject{{QStringLiteral("type"), QStringLiteral("json_object")}}}};
    if (m_operation == Operation::RecipeDna
        && model.contains(QStringLiteral("Qwen3"), Qt::CaseInsensitive)
        && !model.contains(QStringLiteral("VL"), Qt::CaseInsensitive))
        body.insert(QStringLiteral("enable_thinking"), false);

    QJsonArray messages;
    if (visionOperation) {
        QJsonArray content;
        for (const QByteArray &imageBase64 : m_imageBase64List) {
            const QString dataUrl = QStringLiteral("data:image/jpeg;base64,")
                                        + QString::fromLatin1(imageBase64);
            content.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("image_url")},
                                       {QStringLiteral("image_url"), QJsonObject{
                                            {QStringLiteral("url"), dataUrl},
                                            {QStringLiteral("detail"), QStringLiteral("high")}}}});
        }
        content.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("text")},
                                   {QStringLiteral("text"), imagePrompt()}});
        messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                    {QStringLiteral("content"), content}});
    } else {
        messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                    {QStringLiteral("content"), dnaPrompt()}});
    }
    body.insert(QStringLiteral("messages"), messages);
    post(QUrl(QStringLiteral("https://api.siliconflow.cn/v1/chat/completions")), body,
         QByteArray("Bearer ") + key.toUtf8());
}

void NutritionAiService::startOllama()
{
    m_triedOllama = true;
    m_lastProvider = QStringLiteral("ollama");
    const QString model = configuredString(QStringLiteral("ollama_model"),
                                           QStringLiteral("qwen3-vl:8b"));
    const bool visionOperation = m_operation != Operation::RecipeDna;
    const int outputTokens = m_operation == Operation::IngredientVision ? 2400
                           : visionOperation ? 1900 : 1400;
    QJsonObject body{{QStringLiteral("model"), model},
                     {QStringLiteral("stream"), false},
                     {QStringLiteral("format"), QStringLiteral("json")},
                     {QStringLiteral("options"), QJsonObject{
                          {QStringLiteral("temperature"), 0.15},
                          {QStringLiteral("num_predict"), outputTokens}}}};
    QJsonObject userMessage{{QStringLiteral("role"), QStringLiteral("user")},
                            {QStringLiteral("content"), visionOperation
                                ? imagePrompt() : dnaPrompt()}};
    if (visionOperation) {
        QJsonArray images;
        for (const QByteArray &imageBase64 : m_imageBase64List)
            images.append(QString::fromLatin1(imageBase64));
        userMessage.insert(QStringLiteral("images"), images);
    }
    body.insert(QStringLiteral("messages"), QJsonArray{userMessage});
    post(QUrl(QStringLiteral("http://127.0.0.1:11434/api/chat")), body);
}

void NutritionAiService::post(const QUrl &url, const QJsonObject &body,
                              const QByteArray &authorization)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(90000);
    if (!authorization.isEmpty())
        request.setRawHeader("Authorization", authorization);
    m_reply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_timeout->start();
    connect(m_reply, &QNetworkReply::finished, this, &NutritionAiService::handleReply);
}

void NutritionAiService::handleReply()
{
    QNetworkReply *reply = m_reply;
    if (!reply)
        return;
    m_reply = nullptr;
    m_timeout->stop();
    const QByteArray raw = reply->readAll();
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    if (m_cancelled || !isBusy())
        return;

    if (networkError != QNetworkReply::NoError) {
        QString detailedError = responseErrorText(raw, networkErrorText);
        if (statusCode == 401 || statusCode == 403)
            detailedError = QStringLiteral("云端鉴权失败，请检查硅基流动 API Key。%1").arg(detailedError);
        else if (statusCode == 429)
            detailedError = QStringLiteral("云端请求过于频繁或额度不足，请稍后重试。%1").arg(detailedError);
        else if (networkError == QNetworkReply::TimeoutError)
            detailedError = QStringLiteral("云端识别超时，请缩小图片或稍后重试。%1").arg(detailedError);
        const bool transientCloudFailure = m_lastProvider == QLatin1String("siliconflow")
            && statusCode != 401 && statusCode != 403 && statusCode != 429
            && !m_triedOllama;
        if (transientCloudFailure && m_cloudRetryCount < 1) {
            ++m_cloudRetryCount;
            m_lastNetworkError = detailedError;
            startSiliconFlow();
            return;
        }
        if (!m_triedOllama) {
            m_lastNetworkError = detailedError;
            startOllama();
            return;
        }
        const QString detail = m_lastNetworkError.isEmpty()
            ? detailedError
            : QStringLiteral("云端：%1；本地：%2").arg(m_lastNetworkError, detailedError);
        const QString message = QStringLiteral("联网识别未完成，且本地备用模型不可用。请根据详情检查服务后重试：%1")
                                    .arg(detail);
        m_operation != Operation::RecipeDna ? finishFoodError(message) : finishDnaError(message);
        return;
    }

    QString content;
    QString responseError;
    extractChatRoot(raw, m_lastProvider, &content, &responseError);
    if (content.trimmed().isEmpty()) {
        if (m_lastProvider == QLatin1String("siliconflow")
            && !m_triedOllama && m_cloudRetryCount < 1) {
            ++m_cloudRetryCount;
            m_lastNetworkError = responseError;
            startSiliconFlow();
            return;
        }
        if (!m_triedOllama) {
            m_lastNetworkError = responseError;
            startOllama();
            return;
        }
        const QString combinedError = m_lastNetworkError.isEmpty()
            ? responseError
            : QStringLiteral("云端失败：%1；本地失败：%2")
                  .arg(m_lastNetworkError, responseError);
        m_operation != Operation::RecipeDna ? finishFoodError(combinedError)
                                            : finishDnaError(combinedError);
        return;
    }
    if (m_operation != Operation::RecipeDna) {
        FoodVisionResult result = parseFoodResult(content, m_lastProvider);
        if (!result.ok && !m_triedOllama) {
            m_lastNetworkError = result.error;
            startOllama();
            return;
        }
        if (!result.ok) {
            finishFoodError(result.error);
            return;
        }
        finishRequest();
        emit foodAnalysisFinished(result);
    } else {
        RecipeDnaResult result = parseRecipeDnaResult(content, m_lastProvider);
        if (!result.ok && !m_triedOllama) {
            m_lastNetworkError = result.error;
            startOllama();
            return;
        }
        if (!result.ok) {
            finishDnaError(result.error);
            return;
        }
        result.category = normalizedCategory(result.category, m_recipe.category);
        result.dishRole = normalizedRole(result.dishRole, m_recipe.dishRole);
        const QString validationError = validateRecipeDnaChange(m_recipe, result, m_instruction);
        if (!validationError.isEmpty()) {
            if (!m_dnaRepairAttempted) {
                m_dnaRepairAttempted = true;
                m_dnaValidationError = validationError;
                m_lastNetworkError.clear();
                m_triedOllama = false;
                startSiliconFlow();
                return;
            }
            finishDnaError(QStringLiteral("AI 返回的改造没有达到目标：%1。已自动重试一次，未保存无效方案。")
                               .arg(validationError));
            return;
        }
        finishRequest();
        emit recipeTransformFinished(result);
    }
}

void NutritionAiService::finishFoodError(const QString &message)
{
    FoodVisionResult result;
    result.error = message.isEmpty() ? QStringLiteral("食物识别失败，请重试。") : message;
    finishRequest();
    emit foodAnalysisFinished(result);
}

void NutritionAiService::finishDnaError(const QString &message)
{
    RecipeDnaResult result;
    result.error = message.isEmpty() ? QStringLiteral("食谱改造失败，请重试。") : message;
    finishRequest();
    emit recipeTransformFinished(result);
}

void NutritionAiService::finishRequest()
{
    const bool wasBusy = isBusy();
    m_timeout->stop();
    m_operation = Operation::None;
    m_reply = nullptr;
    m_imageBase64List.clear();
    m_userQuestion.clear();
    m_calibrationContext.clear();
    if (wasBusy)
        emit busyChanged(false);
}

FoodVisionResult NutritionAiService::parseFoodResult(const QString &content,
                                                      const QString &provider)
{
    FoodVisionResult result;
    result.provider = provider;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(jsonObjectText(content).toUtf8(), &error);
    QJsonObject object;
    if (document.isObject()) {
        object = document.object();
    } else {
        const QJsonArray recoveredItems = completeItemsFromTruncatedJson(content);
        if (recoveredItems.isEmpty()) {
            result.error = QStringLiteral("识别结果不是合法 JSON：%1").arg(error.errorString());
            return result;
        }
        object.insert(QStringLiteral("items"), recoveredItems);
        object.insert(QStringLiteral("summary"),
                      QStringLiteral("AI 返回内容不完整，已保留其中可确认的食材。"));
    }
    result.summary = object.value(QStringLiteral("summary")).toString().trimmed();
    result.answer = object.value(QStringLiteral("answer")).toString().trimmed();
    for (const QJsonValue &value : object.value(QStringLiteral("assumptions")).toArray()) {
        const QString assumption = value.toString().trimmed();
        if (!assumption.isEmpty())
            result.assumptions.append(assumption);
    }
    const QJsonArray items = object.value(QStringLiteral("items")).toArray();
    if (!items.isEmpty()) {
        for (const QJsonValue &value : items) {
            if (!value.isObject())
                continue;
            const FoodVisionItem item = foodVisionItemFromJson(value.toObject());
            if (isValidFoodVisionItem(item))
                result.items.append(item);
        }
    } else {
        // 兼容已部署模型或历史测试返回的单项 JSON。
        const FoodVisionItem item = foodVisionItemFromJson(object);
        if (isValidFoodVisionItem(item))
            result.items.append(item);
    }
    if (result.items.isEmpty()) {
        result.error = QStringLiteral("模型返回的食物名称、份量或营养数值不完整。");
        return result;
    }

    QStringList names;
    double confidenceSum = 0.0;
    for (const FoodVisionItem &item : result.items) {
        names.append(item.foodName);
        result.servingGrams += item.servingGrams;
        result.servingMinGrams += item.servingMinGrams;
        result.servingMaxGrams += item.servingMaxGrams;
        result.calories += item.calories;
        result.protein += item.protein;
        result.carbs += item.carbs;
        result.fat += item.fat;
        confidenceSum += item.confidence;
        result.assumptions.append(item.assumptions);
    }
    const FoodVisionItem &first = result.items.first();
    result.foodName = names.join(QStringLiteral("、"));
    result.itemType = result.items.size() == 1 ? first.itemType : QStringLiteral("多项");
    result.category = result.items.size() == 1 ? first.category : QStringLiteral("混合");
    result.taste = result.items.size() == 1 ? first.taste : QString();
    result.commonUses = result.items.size() == 1 ? first.commonUses : QString();
    result.nutritionHighlights = result.items.size() == 1 ? first.nutritionHighlights : QString();
    result.confidence = confidenceSum / result.items.size();
    result.calibrationBasis = result.items.size() == 1
        ? first.calibrationBasis : QStringLiteral("按各识别项目区间合计");
    if (result.summary.isEmpty() && result.items.size() == 1)
        result.summary = first.summary;
    result.assumptions.removeDuplicates();
    result.ok = true;
    return result;
}

QString NutritionAiService::validateRecipeDnaChange(const Recipe &original,
                                                      const RecipeDnaResult &result,
                                                      const QString &instruction)
{
    if (!result.ok)
        return result.error.isEmpty() ? QStringLiteral("改造结果无效") : result.error;

    QHash<QString, double> before;
    QHash<QString, double> after;
    for (const RecipeIngredient &ingredient : original.ingredients)
        before[ingredientKey(ingredient.foodName)] += ingredient.quantity;
    for (const RecipeDnaIngredient &ingredient : result.ingredients)
        after[ingredientKey(ingredient.name)] += ingredient.quantity;

    bool ingredientChanged = false;
    QSet<QString> keys;
    for (auto it = before.cbegin(); it != before.cend(); ++it)
        keys.insert(it.key());
    for (auto it = after.cbegin(); it != after.cend(); ++it)
        keys.insert(it.key());
    for (const QString &key : keys) {
        const double oldQty = before.value(key);
        const double newQty = after.value(key);
        const double threshold = qMax(1.0, oldQty * 0.08);
        if ((oldQty <= 0.0 && newQty >= 3.0) || (newQty <= 0.0 && oldQty >= 3.0)
            || qAbs(newQty - oldQty) >= threshold) {
            ingredientChanged = true;
            break;
        }
    }
    if (!ingredientChanged)
        return QStringLiteral("原料种类和克数与原食谱实质相同");

    const QString target = instruction.trimmed();
    const QStringList sweetFoods = {QStringLiteral("糖"), QStringLiteral("蜂蜜"),
                                    QStringLiteral("糖浆"), QStringLiteral("炼乳"),
                                    QStringLiteral("甜味剂"), QStringLiteral("果酱"),
                                    QStringLiteral("香蕉"), QStringLiteral("红枣")};
    const QStringList proteinFoods = {QStringLiteral("蛋"), QStringLiteral("奶"),
                                      QStringLiteral("肉"), QStringLiteral("鱼"),
                                      QStringLiteral("虾"), QStringLiteral("蟹"),
                                      QStringLiteral("豆"), QStringLiteral("酸奶"),
                                      QStringLiteral("蛋白粉"), QStringLiteral("坚果")};
    auto groupIncreased = [&](const QStringList &foods) {
        double oldQty = 0.0;
        double newQty = 0.0;
        for (auto it = before.cbegin(); it != before.cend(); ++it) {
            if (containsAny(it.key(), foods))
                oldQty += it.value();
        }
        for (auto it = after.cbegin(); it != after.cend(); ++it) {
            if (containsAny(it.key(), foods))
                newQty += it.value();
        }
        return newQty >= oldQty + qMax(2.0, oldQty * 0.08);
    };

    if (target.contains(QStringLiteral("甜")) && !groupIncreased(sweetFoods))
        return QStringLiteral("提高甜度目标没有增加或新增甜味原料");

    QList<QPair<QString, double>> beforeFlavorIngredients;
    QList<QPair<QString, double>> afterFlavorIngredients;
    double beforeWeight = 0.0;
    double afterWeight = 0.0;
    for (const RecipeIngredient &ingredient : original.ingredients) {
        beforeFlavorIngredients.append({ingredient.foodName, ingredient.quantity});
        beforeWeight += ingredient.quantity;
    }
    for (const RecipeDnaIngredient &ingredient : result.ingredients) {
        afterFlavorIngredients.append({ingredient.name, ingredient.quantity});
        afterWeight += ingredient.quantity;
    }
    const FlavorFingerprintService flavorService;
    const FlavorFingerprint beforeFlavor = flavorService.estimate(
        original.name, beforeFlavorIngredients, original.steps, original.totalFat, beforeWeight);
    const FlavorFingerprint afterFlavor = flavorService.estimate(
        result.name, afterFlavorIngredients, result.steps, result.fat, afterWeight);
    if (target.contains(QStringLiteral("甜"))
        && afterFlavor.sweet < beforeFlavor.sweet + 4.0)
        return QStringLiteral("风味指纹中的甜度没有明显提高");
    if (containsAny(target, {QStringLiteral("保持风味"), QStringLiteral("保留风味"),
                             QStringLiteral("主要风味")})
        && FlavorFingerprintService::similarity(beforeFlavor, afterFlavor) < 65)
        return QStringLiteral("改造后风味指纹与原版相似度低于65%");

    if (target.contains(QStringLiteral("蛋白"))) {
        if (!groupIncreased(proteinFoods))
            return QStringLiteral("提高蛋白质目标没有增加或新增蛋白质原料");
        if (original.totalProtein > 0.0 && result.protein < original.totalProtein * 1.08)
            return QStringLiteral("估算蛋白质未比原食谱提高至少8%");
    }

    const bool reduceFat = target.contains(QStringLiteral("脂肪"))
                           && containsAny(target, {QStringLiteral("减"), QStringLiteral("少"),
                                                   QStringLiteral("降低"), QStringLiteral("-")});
    if (reduceFat && original.totalFat > 0.0) {
        const double requiredRatio = target.contains(QStringLiteral("30")) ? 0.75 : 0.90;
        if (result.fat > original.totalFat * requiredRatio)
            return target.contains(QStringLiteral("30"))
                ? QStringLiteral("估算脂肪未下降至少25%")
                : QStringLiteral("估算脂肪没有明显下降");
    }
    return {};
}

RecipeDnaResult NutritionAiService::parseRecipeDnaResult(const QString &content,
                                                          const QString &provider)
{
    RecipeDnaResult result;
    result.provider = provider;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(jsonObjectText(content).toUtf8(), &error);
    if (!document.isObject()) {
        result.error = QStringLiteral("改造结果不是合法 JSON：%1").arg(error.errorString());
        return result;
    }
    const QJsonObject object = document.object();
    result.name = object.value(QStringLiteral("name")).toString().trimmed();
    result.category = object.value(QStringLiteral("category")).toString().trimmed();
    result.dishRole = object.value(QStringLiteral("dish_role")).toString().trimmed();
    result.steps = object.value(QStringLiteral("steps")).toString().trimmed();
    result.cookMinutes = qBound(1, object.value(QStringLiteral("cook_minutes")).toInt(20), 480);
    result.changeSummary = object.value(QStringLiteral("change_summary")).toString().trimmed();
    const QJsonObject nutrition = object.value(QStringLiteral("estimated_nutrition")).toObject();
    result.calories = number(nutrition, QStringLiteral("calories"));
    result.protein = number(nutrition, QStringLiteral("protein"));
    result.carbs = number(nutrition, QStringLiteral("carbs"));
    result.fat = number(nutrition, QStringLiteral("fat"));
    for (const QJsonValue &value : object.value(QStringLiteral("ingredients")).toArray()) {
        const QJsonObject ingredientObject = value.toObject();
        RecipeDnaIngredient ingredient;
        ingredient.name = ingredientObject.value(QStringLiteral("name")).toString().trimmed();
        ingredient.quantity = number(ingredientObject, QStringLiteral("grams"));
        ingredient.quantityText = consistentQuantityText(
            ingredientObject.value(QStringLiteral("display")).toString(), ingredient.quantity);
        if (!ingredient.name.isEmpty() && ingredient.quantity > 0.0)
            result.ingredients.append(ingredient);
    }
    if (result.name.isEmpty() || result.steps.isEmpty() || result.ingredients.isEmpty()
        || result.calories < 0.0 || result.protein < 0.0 || result.carbs < 0.0
        || result.fat < 0.0) {
        result.error = QStringLiteral("模型返回的食谱名称、主料、步骤或营养信息不完整。");
        return result;
    }
    result.ok = true;
    return result;
}
