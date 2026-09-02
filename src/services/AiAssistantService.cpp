#include "AiAssistantService.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

namespace {
QJsonObject readAiConfig()
{
    const QStringList paths = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/data/ai_config.json"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/ai_config.json"),
        QStringLiteral("data/ai_config.json"),
    };
    for (const QString &path : paths) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (doc.isObject())
            return doc.object();
    }
    return {};
}

bool looksCorrupted(const QString &text)
{
    if (text.isEmpty())
        return false;
    if (text.size() > 600)
        return true;
    int quoteRun = 0;
    int maxQuoteRun = 0;
    for (const QChar c : text) {
        if (c == QLatin1Char('"') || c == QLatin1Char('“') || c == QLatin1Char('”')) {
            ++quoteRun;
            maxQuoteRun = qMax(maxQuoteRun, quoteRun);
        } else {
            quoteRun = 0;
        }
    }
    if (maxQuoteRun >= 8)
        return true;
    if (text.count(QStringLiteral("顿号")) >= 2)
        return true;
    if (text.contains(QStringLiteral("falsefalse")))
        return true;
    return false;
}

QString cleanPlainReply(QString text)
{
    text = text.trimmed();
    // 去掉 markdown 代码块
    text.remove(QRegularExpression(QStringLiteral("```(?:json)?"), QRegularExpression::CaseInsensitiveOption));
    text.remove(QStringLiteral("```"));
    text = text.trimmed();
    if (looksCorrupted(text))
        return {};
    // 若整段仍是 JSON，尝试只取 reply
    if (text.startsWith(QLatin1Char('{')) && text.contains(QStringLiteral("\"reply\""))) {
        const QRegularExpression re(QStringLiteral("\"reply\"\\s*:\\s*\"((?:\\\\.|[^\"\\\\])*)\""));
        const QRegularExpressionMatch m = re.match(text);
        if (m.hasMatch()) {
            QString reply = m.captured(1);
            reply.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
            reply.replace(QStringLiteral("\\\""), QStringLiteral("\""));
            if (!looksCorrupted(reply) && !reply.isEmpty())
                return reply.trimmed();
        }
        return {};
    }
    return text;
}

QString extractFieldByRegex(const QString &text, const QString &key)
{
    const QRegularExpression re(
        QStringLiteral("\"%1\"\\s*:\\s*\"((?:\\\\.|[^\"\\\\])*)\"").arg(QRegularExpression::escape(key)));
    const QRegularExpressionMatch m = re.match(text);
    if (!m.hasMatch())
        return {};
    QString v = m.captured(1);
    v.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    v.replace(QStringLiteral("\\\""), QStringLiteral("\""));
    return v.trimmed();
}

bool extractBoolByRegex(const QString &text, const QString &key, bool defaultValue)
{
    const QRegularExpression re(
        QStringLiteral("\"%1\"\\s*:\\s*(true|false)").arg(QRegularExpression::escape(key)),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(text);
    if (!m.hasMatch())
        return defaultValue;
    return m.captured(1).compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
}

QString sanitizeListField(QString value)
{
    value = value.trimmed();
    if (value.isEmpty() || looksCorrupted(value))
        return {};
    if (value == QStringLiteral("无") || value == QStringLiteral("不改")
        || value == QStringLiteral("不变") || value == QLatin1String("null"))
        return {};
    // 去掉提示性脏词
    value.remove(QStringLiteral("顿号"));
    value.remove(QStringLiteral("或逗号分隔"));
    value.remove(QStringLiteral("空字符串表示不改"));
    value = value.trimmed();
    if (looksCorrupted(value) || value.size() > 120)
        return {};
    return value;
}

QStringList explicitAvoidanceItems(const QString &message)
{
    const bool avoidanceContext = message.contains(QStringLiteral("不喜欢"))
                                  || message.contains(QStringLiteral("不爱吃"))
                                  || message.contains(QStringLiteral("不吃"))
                                  || message.contains(QStringLiteral("不要"))
                                  || message.contains(QStringLiteral("避开"))
                                  || message.contains(QStringLiteral("忌口"))
                                  || message.contains(QStringLiteral("不能吃"));
    if (!avoidanceContext)
        return {};

    QStringList found;
    const QStringList knownFoods = {
        QStringLiteral("豆腐"), QStringLiteral("豆制品"), QStringLiteral("大豆"),
        QStringLiteral("鸡蛋"), QStringLiteral("牛奶"), QStringLiteral("花生"),
        QStringLiteral("坚果"), QStringLiteral("海鲜"), QStringLiteral("虾"),
        QStringLiteral("蟹"), QStringLiteral("鱼"), QStringLiteral("猪肉"),
        QStringLiteral("牛肉"), QStringLiteral("羊肉"), QStringLiteral("鸡肉"),
        QStringLiteral("香菜"), QStringLiteral("芹菜"), QStringLiteral("苦瓜"),
        QStringLiteral("西兰花"), QStringLiteral("辣椒"), QStringLiteral("米饭"),
        QStringLiteral("面条"), QStringLiteral("乳糖"), QStringLiteral("麸质")
    };
    for (const QString &food : knownFoods) {
        if (message.contains(food))
            found.append(food);
    }
    if (found.isEmpty()) {
        const QRegularExpression re(
            QStringLiteral("(?:不喜欢|不爱吃|不吃|不要吃?|避开|忌口|不能吃)\\s*"
                           "([\\x{4e00}-\\x{9fff}]{1,8})"));
        const QRegularExpressionMatch match = re.match(message);
        if (match.hasMatch())
            found.append(match.captured(1));
    }
    found.removeDuplicates();
    return found;
}
} // namespace

AiAssistantService::AiAssistantService(QObject *parent)
    : QObject(parent)
{
}

QString AiAssistantService::loadApiKey() const
{
    const QJsonObject cfg = readAiConfig();
    const QString key = cfg.value(QStringLiteral("siliconflow_api_key")).toString().trimmed();
    if (!key.isEmpty())
        return key;
    return qEnvironmentVariable("SILICONFLOW_API_KEY").trimmed();
}

QString AiAssistantService::siliconModel() const
{
    const QJsonObject cfg = readAiConfig();
    const QString model = cfg.value(QStringLiteral("siliconflow_model")).toString().trimmed();
    return model.isEmpty() ? QStringLiteral("Qwen/Qwen3-8B") : model;
}

QString AiAssistantService::ollamaModel() const
{
    const QJsonObject cfg = readAiConfig();
    const QString model = cfg.value(QStringLiteral("ollama_model")).toString().trimmed();
    return model.isEmpty() ? QStringLiteral("qwen3-vl:8b") : model;
}

QString AiAssistantService::planContextText() const
{
    if (!m_currentPlan.valid) {
        return QStringLiteral("当前尚未生成三餐方案。");
    }
    return QStringLiteral("早餐：%1；午餐：%2；晚餐：%3。摘要：%4")
        .arg(m_currentPlan.breakfast.title(),
             m_currentPlan.lunch.title(),
             m_currentPlan.dinner.title(),
             m_currentPlan.summary);
}

QString AiAssistantService::buildSystemPrompt(const User &user) const
{
    return QStringLiteral(
               "你是「膳衡」智能营养助理。\n"
               "必须只输出一行合法 JSON，不要 Markdown，不要解释。\n"
               "格式：{\"reply\":\"中文回复\",\"goal\":\"\",\"preferences\":\"\",\"allergens\":\"\",\"regenerate\":false}\n"
               "严格规则：\n"
               "0) 必须结合下面提供的最近对话理解上下文；“这食材、它、这个菜”等指代最近一次图片识别或回答中明确出现的对象，不得脱离上下文另猜食材。\n"
               "1) 纯问答（如「XX是什么」「有什么营养」「能不能吃」）只填 reply，goal/preferences/allergens 必须为空字符串，regenerate 必须 false。\n"
               "2) 用户明确改目标/偏好/忌口/过敏，或要求调整三餐/主食（如「习惯白米饭」「午餐换清淡」）时：更新 preferences 或 allergens，且 regenerate=true。\n"
               "3) 「不喜欢/不吃X」属于饮食偏好：preferences 写入「不吃X」，allergens 必须为空；只有明确说「对X过敏」才能写 allergens。\n"
               "4) 回复和新方案绝不能再次推荐用户刚明确排除的食材；先复述已避开的食材，再说明会重新生成。\n"
               "5) 不要因为单纯提问菜名就 regenerate；不要擅自修改档案。\n"
               "- reply：直接回答，说明将按新偏好重新生成方案，120字以内。\n"
               "- goal：仅明确改目标时填 lose/gain/maintain，否则 \"\"。\n"
               "- preferences：改口味/主食/习惯时填完整新偏好列表（含原有合理项），例如「白米饭、清淡」。\n"
               "- allergens：仅明确改忌口/过敏时填完整新列表，否则 \"\"。\n"
               "- regenerate：偏好/忌口/目标有更新，或用户要求换方案时为 true。\n"
               "当前用户：%1，目标=%2，身高=%3cm，体重=%4kg，热量=%5，偏好=%6，忌口=%7，"
               "饮食选择=%8，不耐受=%9，营养缺乏=%10，医疗=%11。\n"
               "当前方案：%12")
        .arg(user.name.isEmpty() ? QStringLiteral("用户") : user.name)
        .arg(user.goal)
        .arg(user.height)
        .arg(user.weight)
        .arg(user.calorieTarget)
        .arg(user.preferences.isEmpty() ? QStringLiteral("无") : user.preferences)
        .arg(user.allergens.isEmpty() ? QStringLiteral("无") : user.allergens)
        .arg(user.dietaryChoices.isEmpty() ? QStringLiteral("无") : user.dietaryChoices.join(QStringLiteral("、")))
        .arg(user.foodIntolerances.isEmpty() ? QStringLiteral("无") : user.foodIntolerances.join(QStringLiteral("、")))
        .arg(user.nutritionalDeficiencies.isEmpty()
                 ? QStringLiteral("无")
                 : user.nutritionalDeficiencies.join(QStringLiteral("、")))
        .arg(user.medicalConditions.isEmpty() ? QStringLiteral("无")
                                              : user.medicalConditions.join(QStringLiteral("、")))
        .arg(planContextText());
}

void AiAssistantService::analyzeUserMessage(const User &user,
                                            const QString &userMessage,
                                            const RecommendResult &currentPlan)
{
    m_pendingUser = user;
    m_pendingMessage = userMessage.trimmed();
    m_currentPlan = currentPlan;
    if (m_pendingMessage.isEmpty()) {
        finishWithError(QStringLiteral("请输入问题，或说明忌口、喜好等需求。"));
        return;
    }

    // 明确的“不喜欢/不吃”属于确定性偏好更新，不需要把用户档案发送到外部模型。
    // 本地先处理既能避免模型答非所问，也能确保被排除食材不会再次出现在回复中。
    const QStringList avoided = explicitAvoidanceItems(m_pendingMessage);
    if (!avoided.isEmpty() && !m_pendingMessage.contains(QStringLiteral("过敏"))) {
        QStringList preferences = User::splitLegacyText(user.preferences);
        for (const QString &item : avoided) {
            for (auto it = preferences.begin(); it != preferences.end();) {
                if (it->contains(item))
                    it = preferences.erase(it);
                else
                    ++it;
            }
            preferences.append(QStringLiteral("不吃%1").arg(item));
        }
        preferences.removeDuplicates();

        AiPreferenceUpdate result;
        result.ok = true;
        result.provider = QStringLiteral("local-rules");
        result.preferences = User::joinLegacyText(preferences);
        result.regenerate = true;
        result.reply = QStringLiteral("已记录你不喜欢「%1」。后续推荐会避开这些食材，并按新偏好重新生成方案。")
                           .arg(avoided.join(QStringLiteral("、")));
        rememberExchange(m_pendingMessage, result.reply);
        emit finished(result);
        return;
    }
    trySiliconFlow(user, m_pendingMessage);
}

void AiAssistantService::trySiliconFlow(const User &user, const QString &userMessage)
{
    const QString apiKey = loadApiKey();
    if (apiKey.isEmpty()) {
        tryOllama(user, userMessage);
        return;
    }

    auto *nam = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl(QStringLiteral("https://api.siliconflow.cn/v1/chat/completions")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey.toUtf8());

    QJsonObject body;
    body.insert(QStringLiteral("model"), siliconModel());
    body.insert(QStringLiteral("temperature"), 0.2);
    body.insert(QStringLiteral("max_tokens"), 320);
    body.insert(QStringLiteral("frequency_penalty"), 0.6);
    body.insert(QStringLiteral("presence_penalty"), 0.3);
    if (siliconModel().contains(QStringLiteral("Qwen3"), Qt::CaseInsensitive))
        body.insert(QStringLiteral("enable_thinking"), false);

    QJsonArray messages;
    messages.append(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("system")},
        {QStringLiteral("content"), buildSystemPrompt(user)},
    });
    for (const auto &exchange : m_history) {
        messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                    {QStringLiteral("content"), exchange.first}});
        messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("assistant")},
                                    {QStringLiteral("content"), exchange.second}});
    }
    messages.append(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("user")},
        {QStringLiteral("content"), userMessage},
    });
    body.insert(QStringLiteral("messages"), messages);

    QNetworkReply *reply = nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, user, userMessage]() {
        reply->deleteLater();
        nam->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            tryOllama(user, userMessage);
            return;
        }

        const QByteArray raw = reply->readAll();
        const QJsonObject root = QJsonDocument::fromJson(raw).object();
        const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty()) {
            tryOllama(user, userMessage);
            return;
        }
        const QString content = choices.at(0)
                                    .toObject()
                                    .value(QStringLiteral("message"))
                                    .toObject()
                                    .value(QStringLiteral("content"))
                                    .toString();
        const AiPreferenceUpdate parsed = parseModelOutput(content, QStringLiteral("siliconflow"));
        if (!parsed.ok || looksCorrupted(parsed.reply)) {
            tryOllama(user, userMessage);
            return;
        }
        rememberExchange(userMessage, parsed.reply);
        emit finished(parsed);
    });
}

void AiAssistantService::tryOllama(const User &user, const QString &userMessage)
{
    auto *nam = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl(QStringLiteral("http://127.0.0.1:11434/api/chat")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QJsonObject body;
    body.insert(QStringLiteral("model"), ollamaModel());
    body.insert(QStringLiteral("stream"), false);
    body.insert(QStringLiteral("options"),
                QJsonObject{{QStringLiteral("temperature"), 0.2},
                            {QStringLiteral("num_predict"), 280}});

    QJsonArray messages;
    messages.append(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("system")},
        {QStringLiteral("content"), buildSystemPrompt(user)},
    });
    for (const auto &exchange : m_history) {
        messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                    {QStringLiteral("content"), exchange.first}});
        messages.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("assistant")},
                                    {QStringLiteral("content"), exchange.second}});
    }
    messages.append(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("user")},
        {QStringLiteral("content"), userMessage},
    });
    body.insert(QStringLiteral("messages"), messages);

    QNetworkReply *reply = nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, userMessage]() {
        reply->deleteLater();
        nam->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            finishWithError(QStringLiteral(
                "硅基流动与本地 Ollama 均不可用。\n"
                "请确认 API Key 或已启动 ollama（%1）。\n详情：%2")
                                .arg(ollamaModel())
                                .arg(reply->errorString()));
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QString content = root.value(QStringLiteral("message"))
                                    .toObject()
                                    .value(QStringLiteral("content"))
                                    .toString();
        if (content.trimmed().isEmpty()) {
            finishWithError(QStringLiteral("模型未返回有效内容，请重试。"));
            return;
        }
        const AiPreferenceUpdate parsed = parseModelOutput(content, QStringLiteral("ollama"));
        if (parsed.ok)
            rememberExchange(userMessage, parsed.reply);
        emit finished(parsed);
    });
}

void AiAssistantService::rememberExchange(const QString &userMessage,
                                          const QString &assistantReply)
{
    const QString userText = userMessage.trimmed();
    const QString answer = assistantReply.trimmed();
    if (userText.isEmpty() || answer.isEmpty())
        return;
    m_history.append(qMakePair(userText, answer.left(1600)));
    while (m_history.size() > 6)
        m_history.removeFirst();
}

void AiAssistantService::resetConversation()
{
    m_history.clear();
}

void AiAssistantService::finishWithError(const QString &error)
{
    AiPreferenceUpdate result;
    result.ok = false;
    result.error = error;
    result.reply = error;
    emit finished(result);
}

AiPreferenceUpdate AiAssistantService::parseModelOutput(const QString &content,
                                                        const QString &provider) const
{
    AiPreferenceUpdate result;
    result.provider = provider;
    result.ok = true;

    const QString trimmed = content.trimmed();
    QString reply = extractFieldByRegex(trimmed, QStringLiteral("reply"));
    if (reply.isEmpty())
        reply = cleanPlainReply(trimmed);

    if (reply.isEmpty() || looksCorrupted(reply)) {
        const QRegularExpression sentenceRe(QStringLiteral("[\\x{4e00}-\\x{9fff}][^\\n\"{}]{4,80}"));
        const QRegularExpressionMatch sm = sentenceRe.match(trimmed);
        if (sm.hasMatch() && !looksCorrupted(sm.captured(0)))
            reply = sm.captured(0).trimmed();
        else
            reply = QStringLiteral("我理解了你的问题。若要改忌口/偏好，请直接说「不吃…」「喜欢…」；"
                                   "若要换方案，请说「重新生成」。");
    }

    result.reply = reply;
    result.goal = extractFieldByRegex(trimmed, QStringLiteral("goal")).trimmed().toLower();
    if (!(result.goal == QLatin1String("lose") || result.goal == QLatin1String("gain")
          || result.goal == QLatin1String("maintain"))) {
        result.goal.clear();
    }
    result.preferences = sanitizeListField(extractFieldByRegex(trimmed, QStringLiteral("preferences")));
    result.allergens = sanitizeListField(extractFieldByRegex(trimmed, QStringLiteral("allergens")));
    result.regenerate = extractBoolByRegex(trimmed, QStringLiteral("regenerate"), false);

    const int start = trimmed.indexOf(QLatin1Char('{'));
    const int end = trimmed.indexOf(QLatin1Char('}'), start + 1);
    if (start >= 0 && end > start) {
        const QJsonObject obj = QJsonDocument::fromJson(trimmed.mid(start, end - start + 1).toUtf8()).object();
        if (!obj.isEmpty()) {
            const QString jsonReply = cleanPlainReply(obj.value(QStringLiteral("reply")).toString());
            if (!jsonReply.isEmpty())
                result.reply = jsonReply;
            const QString g = obj.value(QStringLiteral("goal")).toString().trimmed().toLower();
            if (g == QLatin1String("lose") || g == QLatin1String("gain") || g == QLatin1String("maintain"))
                result.goal = g;
            const QString prefs = sanitizeListField(obj.value(QStringLiteral("preferences")).toString());
            if (!prefs.isEmpty())
                result.preferences = prefs;
            const QString allergens = sanitizeListField(obj.value(QStringLiteral("allergens")).toString());
            if (!allergens.isEmpty())
                result.allergens = allergens;
            if (obj.contains(QStringLiteral("regenerate")))
                result.regenerate = obj.value(QStringLiteral("regenerate")).toBool(false);
        }
    }

    const QString msg = m_pendingMessage;
    const bool asksProfileChange =
        msg.contains(QStringLiteral("忌口")) || msg.contains(QStringLiteral("过敏"))
        || msg.contains(QStringLiteral("不吃")) || msg.contains(QStringLiteral("不要"))
        || msg.contains(QStringLiteral("喜欢")) || msg.contains(QStringLiteral("偏好"))
        || msg.contains(QStringLiteral("目标")) || msg.contains(QStringLiteral("减重"))
        || msg.contains(QStringLiteral("增肌")) || msg.contains(QStringLiteral("换成"))
        || msg.contains(QStringLiteral("换清淡")) || msg.contains(QStringLiteral("太油"))
        || msg.contains(QStringLiteral("习惯")) || msg.contains(QStringLiteral("主食"))
        || msg.contains(QStringLiteral("调整")) || msg.contains(QStringLiteral("改成"))
        || msg.contains(QStringLiteral("换成")) || msg.contains(QStringLiteral("想吃"))
        || msg.contains(QStringLiteral("要用")) || msg.contains(QStringLiteral("改为"));
    const bool asksPlanAdjust =
        msg.contains(QStringLiteral("白米饭"))
        || (msg.contains(QStringLiteral("冰箱"))
            && (msg.contains(QStringLiteral("推荐")) || msg.contains(QStringLiteral("方案"))))
        || (msg.contains(QStringLiteral("主食"))
            && (msg.contains(QStringLiteral("习惯")) || msg.contains(QStringLiteral("换成"))
                || msg.contains(QStringLiteral("改成")) || msg.contains(QStringLiteral("要用"))
                || msg.contains(QStringLiteral("想吃"))))
        || msg.contains(QStringLiteral("换一道")) || msg.contains(QStringLiteral("换一换"))
        || msg.contains(QStringLiteral("调整方案")) || msg.contains(QStringLiteral("重新搭配"));
    const bool asksRegen =
        msg.contains(QStringLiteral("重新生成")) || msg.contains(QStringLiteral("重新推荐"))
        || msg.contains(QStringLiteral("再推荐")) || msg.contains(QStringLiteral("换一套"))
        || msg.contains(QStringLiteral("换个方案")) || msg.contains(QStringLiteral("重新出"))
        || asksPlanAdjust;
    const bool looksLikeQa =
        msg.contains(QStringLiteral("是什么")) || msg.contains(QStringLiteral("什么是"))
        || msg.contains(QStringLiteral("为什么")) || msg.contains(QStringLiteral("怎么做"))
        || ((msg.contains(QStringLiteral("吗")) || msg.contains(QStringLiteral("？"))
             || msg.contains(QLatin1Char('?')))
            && !asksProfileChange && !asksPlanAdjust);

    // 纯问答：强制清空档案改动与 regenerate，避免模型误触重生成
    if (looksLikeQa && !asksProfileChange && !asksRegen) {
        result.goal.clear();
        result.preferences.clear();
        result.allergens.clear();
        result.regenerate = false;
        return result;
    }

    // 本地兜底：用户明确要白米饭作主食等，写入偏好并强制重生成
    if (msg.contains(QStringLiteral("白米饭"))
        && (msg.contains(QStringLiteral("主食")) || msg.contains(QStringLiteral("习惯"))
            || msg.contains(QStringLiteral("午餐")) || msg.contains(QStringLiteral("晚餐"))
            || msg.contains(QStringLiteral("中午")) || msg.contains(QStringLiteral("晚上")))) {
        QStringList prefs = User::splitLegacyText(m_pendingUser.preferences);
        if (!prefs.contains(QStringLiteral("白米饭")))
            prefs.prepend(QStringLiteral("白米饭"));
        result.preferences = User::joinLegacyText(prefs);
        result.regenerate = true;
        if (result.reply.isEmpty() || result.reply.size() < 4)
            result.reply = QStringLiteral("好的，已记录「白米饭」作为午/晚餐主食偏好，正在按此重新生成方案。");
    }

    if (!asksProfileChange && !asksPlanAdjust) {
        // 保留上面白米饭兜底写入的 preferences
        if (!(msg.contains(QStringLiteral("白米饭")))) {
            result.goal.clear();
            result.preferences.clear();
            result.allergens.clear();
        }
    }

    // 对明确的“不喜欢/不吃”意图做本地一致性校验。模型即使错误地继续推荐
    // 被排除食材，也不会把冲突回复展示给用户或写成过敏原。
    const QStringList avoided = explicitAvoidanceItems(msg);
    if (!avoided.isEmpty() && !msg.contains(QStringLiteral("过敏"))) {
        QStringList preferences = User::splitLegacyText(m_pendingUser.preferences);
        for (const QString &item : avoided) {
            for (auto it = preferences.begin(); it != preferences.end();) {
                if (it->contains(item))
                    it = preferences.erase(it);
                else
                    ++it;
            }
            preferences.append(QStringLiteral("不吃%1").arg(item));
        }
        preferences.removeDuplicates();
        result.preferences = User::joinLegacyText(preferences);
        result.allergens.clear();
        result.regenerate = true;
        result.reply = QStringLiteral("已记录你不喜欢「%1」。后续推荐会避开这些食材，并按新偏好重新生成方案。")
                           .arg(avoided.join(QStringLiteral("、")));
    }

    const bool hasProfileUpdate =
        !result.goal.isEmpty() || !result.preferences.isEmpty() || !result.allergens.isEmpty();
    // 仅档案确有更新，或用户明确要求重生成/调整方案时，才允许 regenerate
    result.regenerate = hasProfileUpdate || asksRegen;
    return result;
}
