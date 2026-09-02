#include "HealthDataDAO.h"

#include "DatabaseManager.h"

#include <QHash>
#include <QSqlError>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStringList>
#include <QtGlobal>

namespace {
QString platformName(const QString &platform)
{
    if (platform == QLatin1String("apple_health"))
        return QStringLiteral("Apple 健康");
    if (platform == QLatin1String("health_connect"))
        return QStringLiteral("Android Health Connect");
    return QStringLiteral("通用健康数据");
}
}

bool HealthDataDAO::upsertDailyRecords(int userId, const QList<HealthDailyRecord> &records,
                                       const QString &platform, QString *errorMessage) const
{
    auto fail = [&](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };
    if (userId <= 0 || records.isEmpty() || platform.trimmed().isEmpty())
        return fail(QStringLiteral("用户、平台或健康数据为空。"));

    QSqlDatabase db = DatabaseManager::getInstance().database();
    if (!db.isOpen())
        return fail(QStringLiteral("数据库不可用。"));
    if (!db.transaction())
        return fail(db.lastError().text());

    QDate from;
    QDate to;
    int imported = 0;
    for (const HealthDailyRecord &record : records) {
        if (!record.date.isValid())
            continue;
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "INSERT INTO user_health_daily(user_id,record_date,steps,active_calories,"
            "weight_kg,sleep_hours,source,updated_at) "
            "VALUES(:user,:date,:steps,:active,:weight,:sleep,:source,datetime('now','localtime')) "
            "ON CONFLICT(user_id,record_date,source) DO UPDATE SET "
            "steps=excluded.steps,active_calories=excluded.active_calories,"
            "weight_kg=CASE WHEN excluded.weight_kg>0 THEN excluded.weight_kg ELSE user_health_daily.weight_kg END,"
            "sleep_hours=excluded.sleep_hours,updated_at=datetime('now','localtime')"));
        query.bindValue(QStringLiteral(":user"), userId);
        query.bindValue(QStringLiteral(":date"), record.date.toString(Qt::ISODate));
        query.bindValue(QStringLiteral(":steps"), qMax(0, record.steps));
        query.bindValue(QStringLiteral(":active"), qMax(0.0, record.activeCalories));
        query.bindValue(QStringLiteral(":weight"), qMax(0.0, record.weightKg));
        query.bindValue(QStringLiteral(":sleep"), qBound(0.0, record.sleepHours, 24.0));
        query.bindValue(QStringLiteral(":source"), platform);
        if (!query.exec()) {
            db.rollback();
            return fail(query.lastError().text());
        }
        ++imported;
        if (!from.isValid() || record.date < from)
            from = record.date;
        if (!to.isValid() || record.date > to)
            to = record.date;
    }
    if (imported == 0) {
        db.rollback();
        return fail(QStringLiteral("同步包中没有可写入的日期记录。"));
    }

    QSqlQuery source(db);
    source.prepare(QStringLiteral(
        "INSERT INTO health_sync_sources(user_id,platform,display_name,status,record_count,"
        "from_date,to_date,last_synced_at) "
        "VALUES(:user,:platform,:name,'connected',:count,:from,:to,datetime('now','localtime')) "
        "ON CONFLICT(user_id,platform) DO UPDATE SET display_name=excluded.display_name,"
        "status='connected',record_count=excluded.record_count,from_date=excluded.from_date,"
        "to_date=excluded.to_date,last_synced_at=datetime('now','localtime')"));
    source.bindValue(QStringLiteral(":user"), userId);
    source.bindValue(QStringLiteral(":platform"), platform);
    source.bindValue(QStringLiteral(":name"), platformName(platform));
    source.bindValue(QStringLiteral(":count"), imported);
    source.bindValue(QStringLiteral(":from"), from.toString(Qt::ISODate));
    source.bindValue(QStringLiteral(":to"), to.toString(Qt::ISODate));
    if (!source.exec()) {
        db.rollback();
        return fail(source.lastError().text());
    }
    if (!db.commit())
        return fail(db.lastError().text());
    return true;
}

QList<HealthDailyRecord> HealthDataDAO::dailyRecords(int userId, const QDate &from,
                                                      const QDate &to) const
{
    QList<HealthDailyRecord> result;
    if (userId <= 0 || !from.isValid() || !to.isValid() || from > to)
        return result;

    struct Aggregate {
        int steps = 0;
        double active = 0.0;
        double weight = 0.0;
        double sleep = 0.0;
        QStringList sources;
    };
    QHash<QDate, Aggregate> byDate;
    QSqlQuery query(DatabaseManager::getInstance().database());
    query.prepare(QStringLiteral(
        "SELECT record_date,steps,active_calories,weight_kg,sleep_hours,source "
        "FROM user_health_daily WHERE user_id=:user AND record_date>=:from "
        "AND record_date<=:to ORDER BY record_date,updated_at"));
    query.bindValue(QStringLiteral(":user"), userId);
    query.bindValue(QStringLiteral(":from"), from.toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":to"), to.toString(Qt::ISODate));
    if (!query.exec())
        return result;
    while (query.next()) {
        const QDate date = QDate::fromString(query.value(0).toString(), Qt::ISODate);
        if (!date.isValid())
            continue;
        Aggregate &aggregate = byDate[date];
        // 多个平台可能记录同一次运动，取最大值而不是求和，避免重复计算。
        aggregate.steps = qMax(aggregate.steps, query.value(1).toInt());
        aggregate.active = qMax(aggregate.active, query.value(2).toDouble());
        const double weight = query.value(3).toDouble();
        if (weight > 0.0)
            aggregate.weight = weight;
        aggregate.sleep = qMax(aggregate.sleep, query.value(4).toDouble());
        aggregate.sources.append(query.value(5).toString());
    }
    for (QDate date = from; date <= to; date = date.addDays(1)) {
        const Aggregate aggregate = byDate.value(date);
        if (aggregate.steps <= 0 && aggregate.active <= 0.0 && aggregate.weight <= 0.0
            && aggregate.sleep <= 0.0)
            continue;
        HealthDailyRecord record;
        record.date = date;
        record.steps = aggregate.steps;
        record.activeCalories = aggregate.active;
        record.weightKg = aggregate.weight;
        record.sleepHours = aggregate.sleep;
        record.source = aggregate.sources.join(QStringLiteral("+"));
        result.append(record);
    }
    return result;
}

QList<HealthSourceStatus> HealthDataDAO::sourceStatuses(int userId) const
{
    QList<HealthSourceStatus> result;
    QSqlQuery query(DatabaseManager::getInstance().database());
    query.prepare(QStringLiteral(
        "SELECT platform,display_name,status,record_count,from_date,to_date,last_synced_at "
        "FROM health_sync_sources WHERE user_id=:user ORDER BY last_synced_at DESC"));
    query.bindValue(QStringLiteral(":user"), userId);
    if (!query.exec())
        return result;
    while (query.next()) {
        HealthSourceStatus status;
        status.platform = query.value(0).toString();
        status.displayName = query.value(1).toString();
        status.status = query.value(2).toString();
        status.recordCount = query.value(3).toInt();
        status.fromDate = QDate::fromString(query.value(4).toString(), Qt::ISODate);
        status.toDate = QDate::fromString(query.value(5).toString(), Qt::ISODate);
        status.lastSyncedAt = QDateTime::fromString(query.value(6).toString(), Qt::ISODate);
        if (!status.lastSyncedAt.isValid())
            status.lastSyncedAt = QDateTime::fromString(query.value(6).toString(),
                                                       QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        result.append(status);
    }
    return result;
}
