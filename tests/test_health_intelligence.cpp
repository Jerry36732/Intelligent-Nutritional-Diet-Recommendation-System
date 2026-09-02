#include "../src/entities/User.h"
#include "../src/dao/DatabaseManager.h"
#include "../src/dao/HealthDataDAO.h"
#include "../src/services/AdaptiveTargetService.h"
#include "../src/services/FlavorFingerprintService.h"
#include "../src/services/HealthDataSyncService.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSqlQuery>
#include <QTemporaryDir>

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
    QCoreApplication app(argc, argv);
    User user;
    user.id = 1;
    user.goal = QStringLiteral("lose");
    user.calorieTarget = 2100;
    user.weight = 70.0;
    const QDate today(2026, 9, 1);
    QList<DailyFoodLogPoint> food;
    QList<HealthDailyRecord> health;
    for (int i = 0; i < 14; ++i) {
        DailyFoodLogTotals totals;
        totals.count = 3;
        totals.calories = 2250;
        totals.protein = 105;
        food.append({today.addDays(i - 13), totals});
        HealthDailyRecord record;
        record.date = today.addDays(i - 13);
        record.steps = 8420;
        record.activeCalories = 430;
        record.sleepHours = 7.1;
        if (i == 0)
            record.weightKg = 70.4;
        if (i == 13)
            record.weightKg = 70.0;
        record.source = QStringLiteral("apple_health");
        health.append(record);
    }
    const AdaptiveTargetResult adaptive = AdaptiveTargetService::calculate(user, food, health, 14);
    expect(adaptive.enoughData && adaptive.effectiveTarget == 2250
               && adaptive.decision == QStringLiteral("维持有效方案"),
           "two-week feedback keeps an effective 2250 kcal target");
    expect(adaptive.explanation.contains(QStringLiteral("下降0.4kg"))
               && adaptive.averageSteps == 8420,
           "adaptive explanation retains weight and activity evidence");

    QString error;
    const QList<HealthDailyRecord> parsed = HealthDataSyncService::parseHealthConnectJson(
        QByteArrayLiteral("[{\"type\":\"StepsRecord\",\"startTime\":\"2026-09-01T08:00:00+08:00\",\"count\":8420},"
                          "{\"type\":\"ActiveCaloriesBurnedRecord\",\"startTime\":\"2026-09-01T09:00:00+08:00\",\"energy\":{\"inKilocalories\":430}},"
                          "{\"type\":\"WeightRecord\",\"startTime\":\"2026-09-01T07:00:00+08:00\",\"weight\":{\"inKilograms\":70}},"
                          "{\"type\":\"SleepSessionRecord\",\"startTime\":\"2026-09-01T00:00:00+08:00\",\"endTime\":\"2026-09-01T07:06:00+08:00\"}]"),
        &error);
    expect(parsed.size() == 1 && parsed.first().steps == 8420
               && qAbs(parsed.first().activeCalories - 430.0) < 0.01
               && qAbs(parsed.first().weightKg - 70.0) < 0.01
               && qAbs(parsed.first().sleepHours - 7.1) < 0.01,
           "Health Connect daily records merge correctly");

    const QList<HealthDailyRecord> apple = HealthDataSyncService::parseAppleHealthXml(
        QByteArrayLiteral("<?xml version=\"1.0\"?><HealthData>"
                          "<Record type=\"HKQuantityTypeIdentifierStepCount\" value=\"6000\" startDate=\"2026-09-01 08:00:00 +0800\" endDate=\"2026-09-01 09:00:00 +0800\"/>"
                          "<Record type=\"HKQuantityTypeIdentifierActiveEnergyBurned\" value=\"320\" startDate=\"2026-09-01 09:00:00 +0800\" endDate=\"2026-09-01 10:00:00 +0800\"/>"
                          "<Record type=\"HKQuantityTypeIdentifierBodyMass\" value=\"70.2\" startDate=\"2026-09-01 07:00:00 +0800\" endDate=\"2026-09-01 07:00:00 +0800\"/>"
                          "</HealthData>"), &error);
    expect(apple.size() == 1 && apple.first().steps == 6000
               && qAbs(apple.first().activeCalories - 320.0) < 0.01,
           "Apple Health export records merge correctly");

    FlavorFingerprintService flavorService;
    const FlavorFingerprint before = flavorService.estimate(
        QStringLiteral("红烧肉"), {{QStringLiteral("五花肉"), 220.0},
                                   {QStringLiteral("冰糖"), 15.0},
                                   {QStringLiteral("酱油"), 12.0}},
        QStringLiteral("炒糖色后炖至软糯"), 52.0, 247.0);
    const FlavorFingerprint after = flavorService.estimate(
        QStringLiteral("甜香轻脂红烧肉"), {{QStringLiteral("瘦肉"), 150.0},
                                           {QStringLiteral("冰糖"), 24.0},
                                           {QStringLiteral("香菇"), 70.0},
                                           {QStringLiteral("酱油"), 12.0}},
        QStringLiteral("少油炒香后炖至软糯"), 30.0, 256.0);
    expect(after.sweet > before.sweet
               && FlavorFingerprintService::similarity(before, after) >= 60,
           "flavor fingerprint detects sweetness and similarity");
    expect(FlavorFingerprintService::comparisonSummary(before, after)
               .contains(QStringLiteral("风味相似度")),
           "flavor fingerprint produces an auditable summary");

    QTemporaryDir temporary;
    expect(temporary.isValid()
               && DatabaseManager::getInstance().open(temporary.filePath(QStringLiteral("health.db"))),
           "health intelligence schema opens in a fresh database");
    QSqlQuery createUser(DatabaseManager::getInstance().database());
    expect(createUser.exec(QStringLiteral("INSERT INTO users(name) VALUES('health-test')")),
           "health test user is created");
    const int healthUserId = createUser.lastInsertId().toInt();
    QString databaseError;
    expect(HealthDataDAO().upsertDailyRecords(healthUserId, health,
                                               QStringLiteral("apple_health"), &databaseError),
           "authorized health daily summaries persist by source");
    const QList<HealthDailyRecord> stored = HealthDataDAO().dailyRecords(
        healthUserId, today.addDays(-13), today);
    const QList<HealthSourceStatus> sources = HealthDataDAO().sourceStatuses(healthUserId);
    expect(stored.size() == 14 && sources.size() == 1
               && sources.first().platform == QStringLiteral("apple_health")
               && sources.first().recordCount == 14,
           "health sync status and 14 daily records remain queryable");
    return failures == 0 ? 0 : 1;
}
