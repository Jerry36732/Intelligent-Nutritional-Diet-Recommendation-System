#include "WebRecipeImportService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QTextDocumentFragment>

namespace {
QJsonObject findRecipeObject(const QJsonValue &value)
{
    if (value.isArray()) {
        for (const QJsonValue &child : value.toArray()) {
            const QJsonObject found = findRecipeObject(child);
            if (!found.isEmpty())
                return found;
        }
        return {};
    }
    if (!value.isObject())
        return {};

    const QJsonObject object = value.toObject();
    const QJsonValue type = object.value(QStringLiteral("@type"));
    const bool isRecipe = type.isString()
                              ? type.toString().compare(QStringLiteral("Recipe"), Qt::CaseInsensitive) == 0
                              : type.toArray().contains(QStringLiteral("Recipe"));
    if (isRecipe)
        return object;

    for (const QString &key : {QStringLiteral("@graph"), QStringLiteral("mainEntity"),
                               QStringLiteral("itemListElement")}) {
        const QJsonObject found = findRecipeObject(object.value(key));
        if (!found.isEmpty())
            return found;
    }
    return {};
}

void collectInstructionText(const QJsonValue &value, QStringList *steps)
{
    if (value.isString()) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty())
            steps->append(text);
        return;
    }
    if (value.isArray()) {
        for (const QJsonValue &child : value.toArray())
            collectInstructionText(child, steps);
        return;
    }
    if (!value.isObject())
        return;

    const QJsonObject object = value.toObject();
    const QString text = object.value(QStringLiteral("text")).toString().trimmed();
    if (!text.isEmpty())
        steps->append(text);
    collectInstructionText(object.value(QStringLiteral("itemListElement")), steps);
}

QString plainHtmlText(const QString &html)
{
    return QTextDocumentFragment::fromHtml(html).toPlainText().simplified();
}

QString metaContent(const QString &html, const QString &property)
{
    const QString escaped = QRegularExpression::escape(property);
    const QList<QRegularExpression> patterns = {
        QRegularExpression(QStringLiteral(
            "<meta[^>]+(?:property|name)=[\"']%1[\"'][^>]+content=[\"']([^\"']+)[\"']").arg(escaped),
                           QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(QStringLiteral(
            "<meta[^>]+content=[\"']([^\"']+)[\"'][^>]+(?:property|name)=[\"']%1[\"']").arg(escaped),
                           QRegularExpression::CaseInsensitiveOption),
    };
    for (const QRegularExpression &pattern : patterns) {
        const QRegularExpressionMatch match = pattern.match(html);
        if (match.hasMatch())
            return plainHtmlText(match.captured(1));
    }
    return {};
}

QString pageTitle(const QString &html)
{
    QString title = metaContent(html, QStringLiteral("og:title"));
    if (title.isEmpty()) {
        const QRegularExpression pattern(QStringLiteral("<title[^>]*>([\\s\\S]*?)</title>"),
                                         QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = pattern.match(html);
        if (match.hasMatch())
            title = plainHtmlText(match.captured(1));
    }
    return title.trimmed();
}

int parseMinutes(const QString &duration)
{
    const QRegularExpression iso(QStringLiteral("PT(?:(\\d+)H)?(?:(\\d+)M)?"),
                                 QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = iso.match(duration);
    if (!match.hasMatch())
        return 20;
    return qMax(1, match.captured(1).toInt() * 60 + match.captured(2).toInt());
}

QString categoryLabel(const QString &category)
{
    if (category.contains(QStringLiteral("汤")) || category.contains(QStringLiteral("羹")))
        return QStringLiteral("汤羹");
    if (category.contains(QStringLiteral("甜")) || category.contains(QStringLiteral("糕"))
        || category.contains(QStringLiteral("烘焙")))
        return QStringLiteral("甜品");
    if (category.contains(QStringLiteral("主食")) || category.contains(QStringLiteral("饭"))
        || category.contains(QStringLiteral("面")) || category.contains(QStringLiteral("粥")))
        return QStringLiteral("主食");
    if (category.contains(QStringLiteral("素")))
        return QStringLiteral("素菜");
    return QStringLiteral("荤菜");
}

bool containsAny(const QString &text, const QStringList &markers)
{
    for (const QString &marker : markers) {
        if (text.contains(marker, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

WebRecipeImportResult restrictedResult(const QString &html, const QString &finalUrl, int httpStatus)
{
    WebRecipeImportResult result;
    const QString title = pageTitle(html);
    const QString visible = plainHtmlText(html.left(800000));
    const QString probe = title + QLatin1Char('\n') + finalUrl + QLatin1Char('\n') + visible;

    const QStringList verificationMarkers = {
        QStringLiteral("滑块验证"), QStringLiteral("拖动滑块"), QStringLiteral("请完成验证"),
        QStringLiteral("安全验证"), QStringLiteral("访问验证"), QStringLiteral("人机验证"),
        QStringLiteral("验证码"), QStringLiteral("captcha"), QStringLiteral("geetest"),
        QStringLiteral("verifycenter"), QStringLiteral("challenge-platform"),
        QStringLiteral("cf-chl-"), QStringLiteral("/challenge"),
    };
    const QStringList loginMarkers = {
        QStringLiteral("请先登录"), QStringLiteral("登录后查看"), QStringLiteral("登录后继续"),
        QStringLiteral("手机号登录"), QStringLiteral("账号登录"), QStringLiteral("扫码登录"),
        QStringLiteral("/login"), QStringLiteral("/signin"), QStringLiteral("passport."),
    };
    const QStringList blockedMarkers = {
        QStringLiteral("access denied"), QStringLiteral("forbidden"), QStringLiteral("访问被拒绝"),
        QStringLiteral("请求过于频繁"), QStringLiteral("访问过于频繁"), QStringLiteral("请求异常"),
        QStringLiteral("暂时无法访问"), QStringLiteral("操作频繁"),
    };

    if (containsAny(probe, verificationMarkers)) {
        result.state = WebRecipeImportResult::State::VerificationRequired;
        result.message = QStringLiteral(
            "检测到网页安全验证或滑块。系统不会绕过网站验证；请在浏览器完成验证后复制食谱正文，再粘贴解析。");
        return result;
    }
    if (containsAny(probe, loginMarkers)
        || title.contains(QStringLiteral("登录"), Qt::CaseInsensitive)) {
        result.state = WebRecipeImportResult::State::LoginRequired;
        result.message = QStringLiteral(
            "检测到登录页面。请在浏览器登录后复制食谱正文，再回到这里粘贴解析。");
        return result;
    }
    if (httpStatus == 401 || httpStatus == 403 || httpStatus == 407 || httpStatus == 429
        || containsAny(probe, blockedMarkers)) {
        result.state = WebRecipeImportResult::State::AccessBlocked;
        result.message = QStringLiteral(
            "网页拒绝了程序直接读取。请在浏览器正常打开该页面，复制食谱正文后粘贴解析。");
        return result;
    }
    return result;
}

bool isIngredientHeading(const QString &line)
{
    static const QRegularExpression pattern(
        QStringLiteral("^(?:用料|食材|材料|原料|配料)(?:清单|列表)?(?:\\s*[：:])?$"));
    return pattern.match(line).hasMatch();
}

bool isStepHeading(const QString &line)
{
    static const QRegularExpression pattern(
        QStringLiteral("^(?:详细|具体|完整)?\\s*"
                       "(?:做法|步骤|制作步骤|烹饪步骤|制作方法|烹饪方法)"
                       "(?:\\s*[（(][^）)]{0,30}[）)])?(?:\\s*[：:])?$"));
    return pattern.match(line).hasMatch();
}

bool hasStepPrefix(const QString &line)
{
    static const QRegularExpression pattern(
        QStringLiteral("^\\s*(?:(?:第\\s*(?:\\d+|[一二两三四五六七八九十]+)\\s*步)"
                       "|(?:步骤\\s*(?:\\d+|[一二两三四五六七八九十]+))"
                       "|(?:\\d+\\s*[.、。)）:：]))"));
    return pattern.match(line).hasMatch();
}

QString withoutStepPrefix(QString line)
{
    static const QRegularExpression pattern(
        QStringLiteral("^\\s*(?:(?:第\\s*(?:\\d+|[一二两三四五六七八九十]+)\\s*步)"
                       "|(?:步骤\\s*(?:\\d+|[一二两三四五六七八九十]+))"
                       "|(?:\\d+\\s*[.、。)）]))\\s*[：:]?\\s*"));
    line.remove(pattern);
    return line.trimmed();
}

bool looksLikeIngredient(const QString &line)
{
    static const QRegularExpression amount(
        QStringLiteral("(?:\\d+(?:\\.\\d+)?|[一二两三四五六七八九十半]+)\\s*"
                       "(?:克|g|kg|千克|公斤|毫升|ml|升|个|只|枚|片|块|根|棵|颗|粒|瓣|勺|匙|杯|碗|份|把|包|袋|盒|罐)"),
        QRegularExpression::CaseInsensitiveOption);
    return amount.match(line).hasMatch() || line.endsWith(QStringLiteral("适量"))
           || line.endsWith(QStringLiteral("少许"));
}

bool isQuantityOnly(const QString &line)
{
    static const QRegularExpression pattern(
        QStringLiteral("^(?:(?:\\d+(?:\\.\\d+)?|[一二两三四五六七八九十半]+)\\s*"
                       "(?:克|g|kg|千克|公斤|毫升|ml|升|个|只|枚|片|块|根|棵|颗|粒|瓣|勺|匙|杯|碗|份|把|包|袋|盒|罐)"
                       "|适量|少许)$"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern.match(line).hasMatch();
}

bool looksLikeStep(const QString &line)
{
    if (hasStepPrefix(line))
        return true;
    return line.size() >= 12
           && containsAny(line, {QStringLiteral("加入"), QStringLiteral("放入"), QStringLiteral("倒入"),
                                 QStringLiteral("切成"), QStringLiteral("洗净"), QStringLiteral("翻炒"),
                                 QStringLiteral("煮至"), QStringLiteral("烤至"), QStringLiteral("搅拌")});
}

QString cleanedCopiedLine(QString line)
{
    line = line.trimmed();
    line.replace(QChar(0x00a0), QLatin1Char(' '));
    line.remove(QRegularExpression(QStringLiteral("^[✓✔☑•·]+\\s*")));
    return line.simplified();
}
} // namespace

WebRecipeImportResult WebRecipeImportService::parseHtml(const QByteArray &htmlBytes,
                                                         const QString &finalUrl,
                                                         int httpStatus,
                                                         const QString &contentType)
{
    WebRecipeImportResult result;
    const QString html = QString::fromUtf8(htmlBytes);
    if (html.trimmed().isEmpty()) {
        result.state = WebRecipeImportResult::State::InvalidContent;
        result.message = QStringLiteral("网页没有返回可解析的内容。可改用浏览器复制食谱正文后粘贴解析。");
        return result;
    }

    QJsonObject recipeObject;
    QRegularExpression scriptPattern(
        QStringLiteral("<script[^>]+type=[\"']application/ld\\+json[\"'][^>]*>([\\s\\S]*?)</script>"),
        QRegularExpression::CaseInsensitiveOption);
    auto scripts = scriptPattern.globalMatch(html);
    while (scripts.hasNext() && recipeObject.isEmpty()) {
        const QByteArray json = scripts.next().captured(1).trimmed().toUtf8();
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
        if (parseError.error == QJsonParseError::NoError)
            recipeObject = findRecipeObject(document.isArray() ? QJsonValue(document.array())
                                                               : QJsonValue(document.object()));
    }

    if (!recipeObject.isEmpty()) {
        result.name = plainHtmlText(recipeObject.value(QStringLiteral("name")).toString());
        for (const QJsonValue &value : recipeObject.value(QStringLiteral("recipeIngredient")).toArray()) {
            const QString line = plainHtmlText(value.toString()).trimmed();
            if (!line.isEmpty())
                result.ingredients.append(line);
        }
        collectInstructionText(recipeObject.value(QStringLiteral("recipeInstructions")), &result.steps);
        for (int i = 0; i < result.steps.size(); ++i)
            result.steps[i] = QStringLiteral("%1. %2").arg(i + 1).arg(plainHtmlText(result.steps.at(i)));
        const QString duration = recipeObject.value(QStringLiteral("totalTime")).toString(
            recipeObject.value(QStringLiteral("cookTime")).toString());
        result.minutes = parseMinutes(duration);
        result.category = categoryLabel(
            recipeObject.value(QStringLiteral("recipeCategory")).toVariant().toString());
        if (!result.name.isEmpty() && !result.ingredients.isEmpty() && !result.steps.isEmpty()) {
            result.state = WebRecipeImportResult::State::Complete;
            result.message = QStringLiteral("已识别网页食谱，请核对主料和步骤后保存。");
            return result;
        }
    }

    const WebRecipeImportResult restricted = restrictedResult(html, finalUrl, httpStatus);
    if (restricted.isRestricted())
        return restricted;

    const QString normalizedType = contentType.section(QLatin1Char(';'), 0, 0).trimmed().toLower();
    if (!normalizedType.isEmpty() && normalizedType != QLatin1String("text/html")
        && normalizedType != QLatin1String("application/xhtml+xml")
        && normalizedType != QLatin1String("application/ld+json")) {
        result.state = WebRecipeImportResult::State::InvalidContent;
        result.message = QStringLiteral("链接返回的不是网页内容（%1），无法识别食谱。").arg(normalizedType);
        return result;
    }

    // 完成登录或滑块后，部分网站仍不会输出 Schema.org Recipe，但可见正文已经完整。
    // 将渲染后的正文交给复制文本解析器，作为结构化数据之外的安全回退。
    const QString visibleText = QTextDocumentFragment::fromHtml(html).toPlainText();
    WebRecipeImportResult visibleResult = parseCopiedText(visibleText);
    if (!visibleResult.ingredients.isEmpty() && !visibleResult.steps.isEmpty()) {
        const QString title = pageTitle(html);
        if (visibleResult.name.isEmpty() && !title.isEmpty()) {
            visibleResult.name = title;
            visibleResult.name.remove(QRegularExpression(QStringLiteral("\\s*[-_|—].*$")));
            visibleResult.name = visibleResult.name.trimmed();
        }
        visibleResult.state = visibleResult.name.isEmpty()
                                  ? WebRecipeImportResult::State::Incomplete
                                  : WebRecipeImportResult::State::Complete;
        visibleResult.message = visibleResult.state == WebRecipeImportResult::State::Complete
                                    ? QStringLiteral("已从验证后的网页正文提取食谱，请核对后保存。")
                                    : QStringLiteral("已提取原料和步骤，请补充菜品名称后保存。");
        return visibleResult;
    }

    result.name = pageTitle(html);
    result.name.remove(QRegularExpression(QStringLiteral("\\s*[-_|—].*$")));
    result.name = result.name.trimmed();
    result.state = WebRecipeImportResult::State::Incomplete;
    result.message = QStringLiteral(
        "网页未公开完整的结构化食谱。可在浏览器中复制菜名、原料和步骤，再使用下方的正文解析。");
    return result;
}

WebRecipeImportResult WebRecipeImportService::parseCopiedText(const QString &text)
{
    WebRecipeImportResult result;
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QStringList rawLines = normalized.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QStringList lines;
    for (const QString &rawLine : rawLines) {
        const QString line = cleanedCopiedLine(rawLine);
        if (!line.isEmpty())
            lines.append(line);
    }
    if (lines.isEmpty()) {
        result.state = WebRecipeImportResult::State::InvalidContent;
        result.message = QStringLiteral("没有可解析的正文，请先从浏览器复制食谱内容。");
        return result;
    }

    int ingredientHeading = -1;
    int stepHeading = -1;
    int stepStart = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (ingredientHeading < 0 && isIngredientHeading(lines.at(i)))
            ingredientHeading = i;
        else if (stepHeading < 0 && isStepHeading(lines.at(i))) {
            stepHeading = i;
            stepStart = i + 1;
        }
    }
    if (stepStart < 0 && ingredientHeading >= 0) {
        for (int i = ingredientHeading + 1; i < lines.size(); ++i) {
            if (hasStepPrefix(lines.at(i))) {
                stepStart = i;
                break;
            }
        }
    }

    const int nameLimit = ingredientHeading >= 0 ? ingredientHeading
                         : stepHeading >= 0 ? stepHeading : qMin(lines.size(), 8);
    static const QRegularExpression ignoredName(
        QStringLiteral("^(?:首页|登录|注册|收藏|下载|打开APP|菜谱|食谱|用料|做法|步骤|"
                       "下厨房|豆果美食|美食杰|香哈菜谱|菜谱大全)$"),
        QRegularExpression::CaseInsensitiveOption);
    for (int i = 0; i < nameLimit; ++i) {
        QString candidate = lines.at(i);
        candidate.remove(QRegularExpression(QStringLiteral("\\s*[-_|—].*$")));
        if (candidate.size() >= 2 && candidate.size() <= 50
            && !ignoredName.match(candidate).hasMatch() && !candidate.startsWith(QStringLiteral("http"))) {
            result.name = candidate.trimmed();
            break;
        }
    }

    if (ingredientHeading >= 0) {
        const int end = stepHeading > ingredientHeading ? stepHeading
                      : stepStart > ingredientHeading ? stepStart : lines.size();
        for (int i = ingredientHeading + 1; i < end; ++i) {
            const QString line = lines.at(i);
            if (line == QStringLiteral("食材 用量") || line == QStringLiteral("用料 用量")
                || line == QStringLiteral("食材") || line == QStringLiteral("用量"))
                continue;
            if (line.size() > 80)
                continue;
            if (!looksLikeIngredient(line) && i + 1 < end && isQuantityOnly(lines.at(i + 1))) {
                result.ingredients.append(line + QLatin1Char(' ') + lines.at(i + 1));
                ++i;
            } else {
                result.ingredients.append(line);
            }
        }
    }
    if (stepStart >= 0) {
        QString currentStep;
        auto flushCurrentStep = [&]() {
            if (!currentStep.isEmpty()) {
                result.steps.append(currentStep);
                currentStep.clear();
            }
        };
        for (int i = stepStart; i < lines.size(); ++i) {
            QString line = lines.at(i);
            if (line.startsWith(QStringLiteral("小贴士"))
                || line.startsWith(QStringLiteral("注意事项"))
                || line.startsWith(QStringLiteral("菜谱创建时间"))) {
                flushCurrentStep();
                break;
            }
            if (line.size() < 2)
                continue;
            if (hasStepPrefix(line)) {
                flushCurrentStep();
                currentStep = withoutStepPrefix(line);
            } else if (!currentStep.isEmpty()) {
                currentStep += QLatin1Char(' ');
                currentStep += line;
            } else {
                result.steps.append(line);
            }
        }
        flushCurrentStep();
    }

    const bool inferIngredients = result.ingredients.isEmpty();
    const bool inferSteps = result.steps.isEmpty();
    if (inferIngredients || inferSteps) {
        for (const QString &line : lines) {
            if (isIngredientHeading(line) || isStepHeading(line) || line == result.name)
                continue;
            if (inferIngredients && looksLikeIngredient(line))
                result.ingredients.append(line);
            else if (inferSteps && looksLikeStep(line))
                result.steps.append(line);
        }
    }

    for (int i = 0; i < result.steps.size(); ++i)
        result.steps[i] = QStringLiteral("%1. %2").arg(i + 1).arg(result.steps.at(i));

    if (!result.name.isEmpty() && !result.ingredients.isEmpty() && !result.steps.isEmpty()) {
        result.state = WebRecipeImportResult::State::Complete;
        result.message = QStringLiteral("已从复制的网页正文中提取菜名、原料和步骤，请核对后保存。");
    } else {
        result.state = WebRecipeImportResult::State::Incomplete;
        QStringList missing;
        if (result.name.isEmpty())
            missing.append(QStringLiteral("菜名"));
        if (result.ingredients.isEmpty())
            missing.append(QStringLiteral("原料"));
        if (result.steps.isEmpty())
            missing.append(QStringLiteral("步骤"));
        result.message = QStringLiteral("正文已解析，但仍缺少%1；请在下方补充后保存。")
                             .arg(missing.join(QStringLiteral("、")));
    }
    return result;
}
