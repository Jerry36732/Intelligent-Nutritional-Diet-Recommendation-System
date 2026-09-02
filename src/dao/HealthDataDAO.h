#ifndef HEALTHDATADAO_H
#define HEALTHDATADAO_H

#include "../entities/HealthData.h"

class HealthDataDAO
{
public:
    bool upsertDailyRecords(int userId, const QList<HealthDailyRecord> &records,
                            const QString &platform, QString *errorMessage = nullptr) const;
    QList<HealthDailyRecord> dailyRecords(int userId, const QDate &from,
                                           const QDate &to) const;
    QList<HealthSourceStatus> sourceStatuses(int userId) const;
};

#endif // HEALTHDATADAO_H
