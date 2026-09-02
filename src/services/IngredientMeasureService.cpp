#include "IngredientMeasureService.h"

#include <QHash>
#include <QRegularExpression>
#include <QtMath>

namespace {
double numberFromText(const QString &text)
{
    bool ok = false;
    const double numeric = text.toDouble(&ok);
    if (ok)
        return numeric;
    if (text == QStringLiteral("半"))
        return 0.5;
    static const QHash<QChar, int> digits = {
        {QChar(u'零'), 0}, {QChar(u'〇'), 0}, {QChar(u'一'), 1},
        {QChar(u'二'), 2}, {QChar(u'两'), 2}, {QChar(u'三'), 3},
        {QChar(u'四'), 4}, {QChar(u'五'), 5}, {QChar(u'六'), 6},
        {QChar(u'七'), 7}, {QChar(u'八'), 8}, {QChar(u'九'), 9},
    };
    if (text == QStringLiteral("十"))
        return 10.0;
    const int ten = text.indexOf(QChar(u'十'));
    if (ten >= 0) {
        const int tens = ten == 0 ? 1 : digits.value(text.at(ten - 1), 0);
        const int ones = ten + 1 < text.size() ? digits.value(text.at(ten + 1), 0) : 0;
        return tens * 10.0 + ones;
    }
    if (text.size() == 1 && digits.contains(text.at(0)))
        return digits.value(text.at(0));
    return 0.0;
}

QString formattedGrams(double grams)
{
    const int decimals = qAbs(grams - qRound(grams)) < 0.05 ? 0 : 1;
    return QString::number(grams, 'f', decimals);
}

double ingredientSpecificWeight(const QString &name, const QString &unit)
{
    struct Rule { const char *keyword; const char *unit; double grams; };
    // 这里记录的是厨房可操作的可食部平均重量，并非精密称量值。
    // 茭白按常见净茭约100g/根；品种、大小不同会有自然波动。
    static const Rule rules[] = {
        {"茭白", "根", 100.0}, {"高笋", "根", 100.0},
        {"胡萝卜", "根", 150.0}, {"黄瓜", "根", 200.0},
        {"玉米", "根", 200.0}, {"莴笋", "根", 300.0},
        {"大葱", "根", 100.0}, {"小葱", "根", 5.0},
        {"葱白", "根", 12.0}, {"香蕉", "根", 100.0},
        {"茄子", "个", 250.0}, {"土豆", "个", 180.0},
        {"番茄", "个", 180.0}, {"西红柿", "个", 180.0},
        {"洋葱", "个", 200.0}, {"苹果", "个", 200.0},
        {"柠檬", "个", 100.0}, {"鸡蛋", "个", 50.0},
        {"鸡蛋", "枚", 50.0}, {"鸭蛋", "枚", 65.0},
        {"香菇", "朵", 15.0}, {"口蘑", "朵", 20.0},
        {"大蒜", "瓣", 5.0}, {"蒜", "瓣", 5.0},
        {"生姜", "片", 5.0}, {"姜", "片", 5.0},
        {"面包", "片", 30.0}, {"吐司", "片", 30.0},
        {"芝士", "片", 20.0}, {"豆腐", "块", 50.0},
        {"虾", "只", 15.0}, {"大虾", "只", 25.0},
        {"鸡腿", "只", 120.0}, {"鸡翅", "只", 45.0},
        {"排骨", "块", 35.0},
    };
    for (const Rule &rule : rules) {
        if (unit == QString::fromUtf8(rule.unit) && name.contains(QString::fromUtf8(rule.keyword)))
            return rule.grams;
    }
    return 0.0;
}

double teaspoonWeight(const QString &name)
{
    if (name.contains(QStringLiteral("盐")) || name.contains(QStringLiteral("味精")))
        return 6.0;
    if (name.contains(QStringLiteral("糖")))
        return 4.0;
    if (name.contains(QStringLiteral("油")))
        return 4.6;
    if (name.contains(QStringLiteral("淀粉")) || name.contains(QStringLiteral("面粉"))
        || name.contains(QStringLiteral("胡椒粉")) || name.contains(QStringLiteral("辣椒粉")))
        return 3.0;
    if (name.contains(QStringLiteral("水")) || name.contains(QStringLiteral("酒"))
        || name.contains(QStringLiteral("醋")) || name.contains(QStringLiteral("酱油"))
        || name.contains(QStringLiteral("生抽")) || name.contains(QStringLiteral("老抽")))
        return 5.0;
    return 5.0;
}
} // namespace

double IngredientMeasureService::gramsPerUnit(const QString &ingredientName, const QString &unit)
{
    const double specific = ingredientSpecificWeight(ingredientName.trimmed(), unit);
    if (specific > 0.0)
        return specific;
    if (unit == QStringLiteral("茶勺") || unit == QStringLiteral("茶匙")
        || unit == QStringLiteral("小匙"))
        return teaspoonWeight(ingredientName);
    if (unit == QStringLiteral("汤勺") || unit == QStringLiteral("汤匙")
        || unit == QStringLiteral("大匙"))
        return teaspoonWeight(ingredientName) * 3.0;
    static const QHash<QString, double> defaults = {
        {QStringLiteral("个"), 100.0}, {QStringLiteral("只"), 50.0},
        {QStringLiteral("枚"), 50.0}, {QStringLiteral("片"), 10.0},
        {QStringLiteral("块"), 30.0}, {QStringLiteral("根"), 100.0},
        {QStringLiteral("条"), 100.0}, {QStringLiteral("朵"), 15.0},
        {QStringLiteral("瓣"), 5.0}, {QStringLiteral("张"), 5.0},
        {QStringLiteral("束"), 100.0}, {QStringLiteral("杯"), 240.0},
        {QStringLiteral("碗"), 200.0},
    };
    return defaults.value(unit, 0.0);
}

IngredientMeasureEstimate IngredientMeasureService::parse(const QString &source)
{
    IngredientMeasureEstimate result;
    QString line = source.trimmed();
    line.replace(QRegularExpression(QStringLiteral("[：:,，]+")), QStringLiteral(" "));
    const QRegularExpression pattern(
        QStringLiteral("^(.+?)[\\s]*(\\d+(?:\\.\\d+)?|[〇零一二两三四五六七八九十半]+)\\s*"
                       "(kg|千克|g|克|ml|毫升|个|只|枚|片|块|根|条|朵|瓣|张|束|杯|碗|茶勺|茶匙|小匙|汤勺|汤匙|大匙)"
                       "(?:\\s*约\\s*(\\d+(?:\\.\\d+)?)\\s*(?:g|克))?\\s*$"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = pattern.match(line);
    bool quantityFirst = false;
    if (!match.hasMatch()) {
        const QRegularExpression quantityFirstPattern(
            QStringLiteral("^(\\d+(?:\\.\\d+)?|[〇零一二两三四五六七八九十半]+)\\s*"
                           "(kg|千克|g|克|ml|毫升|个|只|枚|片|块|根|条|朵|瓣|张|束|杯|碗|茶勺|茶匙|小匙|汤勺|汤匙|大匙)"
                           "\\s*(.+?)(?:\\s*约\\s*(\\d+(?:\\.\\d+)?)\\s*(?:g|克))?\\s*$"),
            QRegularExpression::CaseInsensitiveOption);
        match = quantityFirstPattern.match(line);
        quantityFirst = match.hasMatch();
    }
    if (!match.hasMatch())
        return result;

    result.ingredientName = match.captured(quantityFirst ? 3 : 1).trimmed();
    const QString amountText = match.captured(quantityFirst ? 1 : 2);
    QString unit = match.captured(quantityFirst ? 2 : 3).toLower();
    const double amount = numberFromText(amountText);
    if (result.ingredientName.isEmpty() || amount <= 0.0)
        return {};

    if (!match.captured(4).isEmpty()) {
        result.grams = match.captured(4).toDouble();
        result.estimated = true;
    } else if (unit == QLatin1String("kg") || unit == QStringLiteral("千克")) {
        result.grams = amount * 1000.0;
    } else if (unit == QLatin1String("g") || unit == QStringLiteral("克")) {
        result.grams = amount;
    } else if (unit == QLatin1String("ml") || unit == QStringLiteral("毫升")) {
        result.grams = amount;
        result.estimated = true;
    } else {
        const double perUnit = gramsPerUnit(result.ingredientName, unit);
        if (perUnit <= 0.0)
            return {};
        result.grams = amount * perUnit;
        result.estimated = true;
    }

    if (result.estimated && unit != QLatin1String("ml") && unit != QStringLiteral("毫升"))
        result.quantityText = amountText + unit + QStringLiteral("约")
                              + formattedGrams(result.grams) + QStringLiteral("g");
    else if (result.estimated)
        result.quantityText = amountText + unit + QStringLiteral("约")
                              + formattedGrams(result.grams) + QStringLiteral("g");
    else
        result.quantityText = formattedGrams(result.grams) + QStringLiteral("g");
    result.valid = result.grams > 0.0;
    return result;
}
