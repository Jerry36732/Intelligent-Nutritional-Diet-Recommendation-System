#include "HealthDataSyncService.h"

#include "../dao/HealthDataDAO.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QXmlStreamReader>

#include <algorithm>
#include <QStringList>
#include <QtGlobal>

namespace {
QDateTime parseDateTime(QString value)
{
    value = value.trimmed();
    QDateTime dateTime = QDateTime::fromString(value, Qt::ISODate);
    if (!dateTime.isValid())
        dateTime = QDateTime::fromString(value, QStringLiteral("yyyy-MM-dd HH:mm:ss Z"));
    if (!dateTime.isValid())
        dateTime = QDateTime::fromString(value.left(19), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    return dateTime;
}

double nestedNumber(const QJsonValue &value, const QStringList &keys)
{
    if (value.isDouble())
        return value.toDouble();
    if (value.isString())
        return value.toString().toDouble();
    if (!value.isObject())
        return 0.0;
    const QJsonObject object = value.toObject();
    for (const QString &key : keys) {
        const QJsonValue child = object.value(key);
        if (child.isDouble())
            return child.toDouble();
        if (child.isString())
            return child.toString().toDouble();
    }
    for (auto it = object.begin(); it != object.end(); ++it) {
        const double result = nestedNumber(it.value(), keys);
        if (result != 0.0)
            return result;
    }
    return 0.0;
}

QString firstString(const QJsonObject &object, const QStringList &keys)
{
    for (const QString &key : keys) {
        const QString value = object.value(key).toString().trimmed();
        if (!value.isEmpty())
            return value;
    }
    return {};
}

void addRecord(QHash<QDate, HealthDailyRecord> *records, const QDate &date,
               int steps, double active, double weight, double sleep,
               const QString &source)
{
    if (!date.isValid())
        return;
    HealthDailyRecord &record = (*records)[date];
    record.date = date;
    record.steps += qMax(0, steps);
    record.activeCalories += qMax(0.0, active);
    if (weight > 0.0)
        record.weightKg = weight;
    record.sleepHours += qBound(0.0, sleep, 24.0);
    record.source = source;
}

QList<HealthDailyRecord> ordered(const QHash<QDate, HealthDailyRecord> &records)
{
    QList<HealthDailyRecord> result = records.values();
    std::sort(result.begin(), result.end(), [](const HealthDailyRecord &left,
                                               const HealthDailyRecord &right) {
        return left.date < right.date;
    });
    for (HealthDailyRecord &record : result)
        record.sleepHours = qBound(0.0, record.sleepHours, 24.0);
    return result;
}
}

HealthImportResult HealthDataSyncService::importFile(int userId, const QString &platform,
                                                     const QString &filePath) const
{
    HealthImportResult result;
    result.platform = platform;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("无法读取同步文件：%1").arg(file.errorString());
        return result;
    }
    const QByteArray payload = file.readAll();
    QString parseError;
    QList<HealthDailyRecord> records;
    if (platform == QLatin1String("apple_health"))
        records = parseAppleHealthXml(payload, &parseError);
    else if (platform == QLatin1String("health_connect"))
        records = parseHealthConnectJson(payload, &parseError);
    else
        records = parseNormalizedCsv(payload, &parseError);
    if (records.isEmpty()) {
        result.error = parseError.isEmpty()
            ? QStringLiteral("同步文件中没有步数、活动消耗、体重或睡眠记录。") : parseError;
        return result;
    }

    QString databaseError;
    if (!HealthDataDAO().upsertDailyRecords(userId, records, platform, &databaseError)) {
        result.error = QStringLiteral("健康数据写入失败：%1").arg(databaseError);
        return result;
    }
    result.ok = true;
    result.importedDays = records.size();
    result.fromDate = records.first().date;
    result.toDate = records.last().date;
    result.message = QStringLiteral("已同步 %1 天（%2 至 %3）")
                         .arg(records.size())
                         .arg(result.fromDate.toString(QStringLiteral("M月d日")))
                         .arg(result.toDate.toString(QStringLiteral("M月d日")));
    return result;
}

QList<HealthDailyRecord> HealthDataSyncService::parseAppleHealthXml(
    const QByteArray &xml, QString *errorMessage)
{
    QHash<QDate, HealthDailyRecord> records;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement() || reader.name() != QLatin1String("Record"))
            continue;
        const QXmlStreamAttributes attributes = reader.attributes();
        const QString type = attributes.value(QStringLiteral("type")).toString();
        const QString valueText = attributes.value(QStringLiteral("value")).toString();
        const QString unit = attributes.value(QStringLiteral("unit")).toString().toLower();
        const QDateTime start = parseDateTime(
            attributes.value(QStringLiteral("startDate")).toString());
        const QDateTime end = parseDateTime(
            attributes.value(QStringLiteral("endDate")).toString());
        const QDate date = start.isValid() ? start.date() : end.date();
        const double value = valueText.toDouble();
        if (type.contains(QStringLiteral("StepCount"))) {
            addRecord(&records, date, qRound(value), 0, 0, 0,
                      QStringLiteral("apple_health"));
        } else if (type.contains(QStringLiteral("ActiveEnergyBurned"))) {
            const double kcal = unit.contains(QStringLiteral("kj")) ? value / 4.184 : value;
            addRecord(&records, date, 0, kcal, 0, 0, QStringLiteral("apple_health"));
        } else if (type.contains(QStringLiteral("BodyMass"))) {
            double kilograms = value;
            if (unit.contains(QStringLiteral("lb")))
                kilograms *= 0.45359237;
            addRecord(&records, date, 0, 0, kilograms, 0,
                      QStringLiteral("apple_health"));
        } else if (type.contains(QStringLiteral("SleepAnalysis"))
                   && valueText.contains(QStringLiteral("Asleep"), Qt::CaseInsensitive)
                   && start.isValid() && end.isValid() && end > start) {
            addRecord(&records, date, 0, 0, 0, start.secsTo(end) / 3600.0,
                      QStringLiteral("apple_health"));
        }
    }
    if (reader.hasError() && errorMessage)
        *errorMessage = QStringLiteral("Apple 健康 XML 解析失败：%1").arg(reader.errorString());
    return ordered(records);
}

void HealthDataSyncService::collectHealthConnectObjects(const QJsonValue &value,
                                                         QList<QJsonValue> *objects)
{
    if (value.isArray()) {
        for (const QJsonValue &child : value.toArray())
            collectHealthConnectObjects(child, objects);
        return;
    }
    if (!value.isObject())
        return;
    const QJsonObject object = value.toObject();
    const QString type = firstString(object, {QStringLiteral("type"),
                                               QStringLiteral("recordType"),
                                               QStringLiteral("dataType")});
    if (!type.isEmpty())
        objects->append(value);
    for (auto it = object.begin(); it != object.end(); ++it)
        collectHealthConnectObjects(it.value(), objects);
}

QList<HealthDailyRecord> HealthDataSyncService::parseHealthConnectJson(
    const QByteArray &json, QString *errorMessage)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (document.isNull()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Health Connect JSON 解析失败：%1")
                                .arg(parseError.errorString());
        return {};
    }
    QList<QJsonValue> objects;
    collectHealthConnectObjects(document.isArray() ? QJsonValue(document.array())
                                                    : QJsonValue(document.object()), &objects);
    QHash<QDate, HealthDailyRecord> records;
    for (const QJsonValue &value : objects) {
        const QJsonObject object = value.toObject();
        const QString type = firstString(object, {QStringLiteral("type"),
                                                   QStringLiteral("recordType"),
                                                   QStringLiteral("dataType")});
        const QDateTime start = parseDateTime(firstString(
            object, {QStringLiteral("startTime"), QStringLiteral("start_time"),
                     QStringLiteral("time")}));
        const QDateTime end = parseDateTime(firstString(
            object, {QStringLiteral("endTime"), QStringLiteral("end_time")}));
        const QDate date = start.isValid() ? start.date()
            : QDate::fromString(firstString(object, {QStringLiteral("date"),
                                                     QStringLiteral("recordDate")}), Qt::ISODate);
        if (type.contains(QStringLiteral("StepsRecord"), Qt::CaseInsensitive)) {
            const int count = qRound(nestedNumber(object.value(QStringLiteral("count")),
                                                  {QStringLiteral("value")}));
            addRecord(&records, date, count, 0, 0, 0,
                      QStringLiteral("health_connect"));
        } else if (type.contains(QStringLiteral("ActiveCaloriesBurnedRecord"), Qt::CaseInsensitive)
                   || type.contains(QStringLiteral("TotalCaloriesBurnedRecord"), Qt::CaseInsensitive)) {
            double kcal = nestedNumber(object.value(QStringLiteral("energy")),
                                       {QStringLiteral("inKilocalories"), QStringLiteral("kilocalories"),
                                        QStringLiteral("kcal"), QStringLiteral("value")});
            if (kcal <= 0.0)
                kcal = nestedNumber(object.value(QStringLiteral("activeCalories")),
                                    {QStringLiteral("value")});
            addRecord(&records, date, 0, kcal, 0, 0,
                      QStringLiteral("health_connect"));
        } else if (type.contains(QStringLiteral("WeightRecord"), Qt::CaseInsensitive)) {
            double weight = nestedNumber(object.value(QStringLiteral("weight")),
                                         {QStringLiteral("inKilograms"), QStringLiteral("kilograms"),
                                          QStringLiteral("kg"), QStringLiteral("value")});
            addRecord(&records, date, 0, 0, weight, 0,
                      QStringLiteral("health_connect"));
        } else if (type.contains(QStringLiteral("SleepSessionRecord"), Qt::CaseInsensitive)
                   && start.isValid() && end.isValid() && end > start) {
            addRecord(&records, date, 0, 0, 0, start.secsTo(end) / 3600.0,
                      QStringLiteral("health_connect"));
        }
    }
    return ordered(records);
}

QList<HealthDailyRecord> HealthDataSyncService::parseNormalizedCsv(
    const QByteArray &csv, QString *errorMessage)
{
    const QList<QByteArray> lines = csv.split('\n');
    if (lines.isEmpty())
        return {};
    const QList<QByteArray> header = lines.first().trimmed().toLower().split(',');
    auto column = [&](const QByteArray &name) { return header.indexOf(name); };
    const int dateColumn = column("date");
    if (dateColumn < 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("通用 CSV 必须包含 date 列。");
        return {};
    }
    QHash<QDate, HealthDailyRecord> records;
    for (int i = 1; i < lines.size(); ++i) {
        const QList<QByteArray> fields = lines.at(i).trimmed().split(',');
        if (dateColumn >= fields.size())
            continue;
        const QDate date = QDate::fromString(QString::fromUtf8(fields.at(dateColumn)).trimmed(),
                                             Qt::ISODate);
        auto numberAt = [&](const QByteArray &name) {
            const int index = column(name);
            return index >= 0 && index < fields.size() ? fields.at(index).trimmed().toDouble() : 0.0;
        };
        addRecord(&records, date, qRound(numberAt("steps")), numberAt("active_calories"),
                  numberAt("weight_kg"), numberAt("sleep_hours"),
                  QStringLiteral("manual_csv"));
    }
    return ordered(records);
}
