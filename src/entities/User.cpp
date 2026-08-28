#include "User.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

QString User::stringListToJson(const QStringList &list)
{
    QJsonArray arr;
    for (const QString &s : list) {
        const QString t = s.trimmed();
        if (!t.isEmpty())
            arr.append(t);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QStringList User::stringListFromJson(const QString &json)
{
    QStringList out;
    const QString trimmed = json.trimmed();
    if (trimmed.isEmpty())
        return out;

    // 兼容旧逗号文本
    if (!trimmed.startsWith(QLatin1Char('[')))
        return splitLegacyText(trimmed);

    const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8());
    if (!doc.isArray())
        return splitLegacyText(trimmed);

    for (const QJsonValue &v : doc.array()) {
        const QString s = v.toString().trimmed();
        if (!s.isEmpty())
            out.append(s);
    }
    return out;
}

QStringList User::splitLegacyText(const QString &text)
{
    QStringList out;
    QString normalized = text;
    normalized.replace(QChar(0x3001), QLatin1Char(',')); // 、
    normalized.replace(QChar(0xff0c), QLatin1Char(',')); // ，
    normalized.replace(QLatin1Char(';'), QLatin1Char(','));
    normalized.replace(QChar(0xff1b), QLatin1Char(',')); // ；
    for (const QString &part : normalized.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString t = part.trimmed();
        if (!t.isEmpty() && !out.contains(t))
            out.append(t);
    }
    return out;
}

QString User::joinLegacyText(const QStringList &list)
{
    QStringList clean;
    for (const QString &s : list) {
        const QString t = s.trimmed();
        if (!t.isEmpty() && !clean.contains(t))
            clean.append(t);
    }
    return clean.join(QStringLiteral("、"));
}

void User::syncAllergenFields()
{
    if (allergies.isEmpty() && !allergens.trimmed().isEmpty())
        allergies = splitLegacyText(allergens);
    if (!allergies.isEmpty())
        allergens = joinLegacyText(allergies);
}

QStringList User::avoidanceKeywords() const
{
    QSet<QString> set;
    for (const QString &s : allergies) {
        const QString t = s.trimmed();
        if (!t.isEmpty())
            set.insert(t);
    }
    for (const QString &s : foodIntolerances) {
        const QString t = s.trimmed();
        if (!t.isEmpty())
            set.insert(t);
    }
    for (const QString &s : splitLegacyText(allergens))
        set.insert(s);
    return set.values();
}

QJsonObject User::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), id);
    o.insert(QStringLiteral("name"), name);
    o.insert(QStringLiteral("gender"), gender);
    o.insert(QStringLiteral("goal"), goal);
    o.insert(QStringLiteral("height"), height);
    o.insert(QStringLiteral("weight"), weight);
    o.insert(QStringLiteral("calorieTarget"), calorieTarget);
    o.insert(QStringLiteral("preferences"), preferences);
    o.insert(QStringLiteral("allergens"), allergens);

    auto toArr = [](const QStringList &list) {
        QJsonArray a;
        for (const QString &s : list) {
            if (!s.trimmed().isEmpty())
                a.append(s.trimmed());
        }
        return a;
    };
    o.insert(QStringLiteral("dietaryChoices"), toArr(dietaryChoices));
    o.insert(QStringLiteral("foodIntolerances"), toArr(foodIntolerances));
    o.insert(QStringLiteral("nutritionalDeficiencies"), toArr(nutritionalDeficiencies));
    o.insert(QStringLiteral("allergies"), toArr(allergies));
    o.insert(QStringLiteral("medicalConditions"), toArr(medicalConditions));
    return o;
}

void User::fromJson(const QJsonObject &obj)
{
    id = obj.value(QStringLiteral("id")).toInt(id);
    name = obj.value(QStringLiteral("name")).toString(name);
    gender = obj.value(QStringLiteral("gender")).toString(gender);
    goal = obj.value(QStringLiteral("goal")).toString(goal);
    height = obj.value(QStringLiteral("height")).toDouble(height);
    weight = obj.value(QStringLiteral("weight")).toDouble(weight);
    calorieTarget = obj.value(QStringLiteral("calorieTarget")).toInt(calorieTarget);
    preferences = obj.value(QStringLiteral("preferences")).toString(preferences);
    allergens = obj.value(QStringLiteral("allergens")).toString(allergens);

    auto fromArr = [](const QJsonValue &v) {
        QStringList list;
        if (v.isArray()) {
            for (const QJsonValue &item : v.toArray()) {
                const QString s = item.toString().trimmed();
                if (!s.isEmpty())
                    list.append(s);
            }
        } else if (v.isString()) {
            list = stringListFromJson(v.toString());
        }
        return list;
    };

    if (obj.contains(QStringLiteral("dietaryChoices")))
        dietaryChoices = fromArr(obj.value(QStringLiteral("dietaryChoices")));
    if (obj.contains(QStringLiteral("foodIntolerances")))
        foodIntolerances = fromArr(obj.value(QStringLiteral("foodIntolerances")));
    if (obj.contains(QStringLiteral("nutritionalDeficiencies")))
        nutritionalDeficiencies = fromArr(obj.value(QStringLiteral("nutritionalDeficiencies")));
    if (obj.contains(QStringLiteral("allergies")))
        allergies = fromArr(obj.value(QStringLiteral("allergies")));
    if (obj.contains(QStringLiteral("medicalConditions")))
        medicalConditions = fromArr(obj.value(QStringLiteral("medicalConditions")));

    syncAllergenFields();
}
