#ifndef HEALTHDATASYNCSERVICE_H
#define HEALTHDATASYNCSERVICE_H

#include "../entities/HealthData.h"

class QJsonValue;

class HealthDataSyncService
{
public:
    HealthImportResult importFile(int userId, const QString &platform,
                                  const QString &filePath) const;

    static QList<HealthDailyRecord> parseAppleHealthXml(const QByteArray &xml,
                                                         QString *errorMessage = nullptr);
    static QList<HealthDailyRecord> parseHealthConnectJson(const QByteArray &json,
                                                            QString *errorMessage = nullptr);
    static QList<HealthDailyRecord> parseNormalizedCsv(const QByteArray &csv,
                                                        QString *errorMessage = nullptr);

private:
    static void collectHealthConnectObjects(const QJsonValue &value,
                                            QList<QJsonValue> *objects);
};

#endif // HEALTHDATASYNCSERVICE_H
