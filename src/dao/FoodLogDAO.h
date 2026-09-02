#ifndef FOODLOGDAO_H
#define FOODLOGDAO_H

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>

struct FoodLogEntry
{
    int id = 0;
    int userId = 0;
    QDateTime eatenAt;
    QString mealLabel;
    QString foodName;
    double servingGrams = 0.0;
    double calories = 0.0;
    double protein = 0.0;
    double carbs = 0.0;
    double fat = 0.0;
    double confidence = 0.0;
    QString provider;
    QString imagePath;
    QString notes;
};

struct DailyFoodLogTotals
{
    int count = 0;
    double calories = 0.0;
    double protein = 0.0;
    double carbs = 0.0;
    double fat = 0.0;
};

struct DailyFoodLogPoint
{
    QDate date;
    DailyFoodLogTotals totals;
};

class FoodLogDAO
{
public:
    int create(FoodLogEntry entry, const QString &sourceImagePath = {},
               QString *errorMessage = nullptr) const;
    bool remove(int id, int userId, QString *errorMessage = nullptr) const;
    QList<FoodLogEntry> recentByUser(int userId, int limit = 20) const;
    DailyFoodLogTotals totalsForDate(int userId, const QDate &date) const;
    QList<DailyFoodLogPoint> dailyTotals(int userId, const QDate &from,
                                         const QDate &to) const;

private:
    QString persistImage(int userId, const QString &sourcePath, QString *errorMessage) const;
};

#endif // FOODLOGDAO_H
