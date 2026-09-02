#ifndef HEALTHDATA_H
#define HEALTHDATA_H

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>

struct HealthDailyRecord
{
    QDate date;
    int steps = 0;
    double activeCalories = 0.0;
    double weightKg = 0.0;
    double sleepHours = 0.0;
    QString source;
};

struct HealthSourceStatus
{
    QString platform;
    QString displayName;
    QString status;
    int recordCount = 0;
    QDate fromDate;
    QDate toDate;
    QDateTime lastSyncedAt;
};

struct HealthImportResult
{
    bool ok = false;
    QString platform;
    int importedDays = 0;
    QDate fromDate;
    QDate toDate;
    QString message;
    QString error;
};

struct AdaptiveTargetResult
{
    bool enoughData = false;
    int windowDays = 14;
    int baseTarget = 0;
    int effectiveTarget = 0;
    int foodLogDays = 0;
    int healthDays = 0;
    double averageIntake = 0.0;
    double weightChangeKg = 0.0;
    double weeklyWeightChangeKg = 0.0;
    double averageSteps = 0.0;
    double averageActiveCalories = 0.0;
    double averageSleepHours = 0.0;
    double confidence = 0.0;
    QString decision;
    QString explanation;
};

#endif // HEALTHDATA_H
