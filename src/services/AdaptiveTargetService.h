#ifndef ADAPTIVETARGETSERVICE_H
#define ADAPTIVETARGETSERVICE_H

#include "../dao/FoodLogDAO.h"
#include "../entities/HealthData.h"
#include "../entities/User.h"

class AdaptiveTargetService
{
public:
    AdaptiveTargetResult analyze(const User &user, int windowDays = 14,
                                 const QDate &today = QDate::currentDate()) const;

    static AdaptiveTargetResult calculate(const User &user,
                                           const QList<DailyFoodLogPoint> &foodPoints,
                                           const QList<HealthDailyRecord> &healthRecords,
                                           int windowDays);
};

#endif // ADAPTIVETARGETSERVICE_H
