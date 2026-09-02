#include "AdaptiveTargetService.h"

#include "UserService.h"
#include "../dao/HealthDataDAO.h"

#include <QtMath>

namespace {
int roundedTarget(double value)
{
    return qBound(1100, qRound(value / 10.0) * 10, 4800);
}

QString trendText(double change)
{
    if (qAbs(change) < 0.05)
        return QStringLiteral("基本稳定");
    return change < 0.0 ? QStringLiteral("下降%1kg").arg(qAbs(change), 0, 'f', 1)
                        : QStringLiteral("上升%1kg").arg(change, 0, 'f', 1);
}
}

AdaptiveTargetResult AdaptiveTargetService::analyze(const User &user, int windowDays,
                                                     const QDate &today) const
{
    windowDays = windowDays <= 7 ? 7 : windowDays <= 14 ? 14 : 30;
    const QDate from = today.addDays(1 - windowDays);
    return calculate(user, FoodLogDAO().dailyTotals(user.id, from, today),
                     HealthDataDAO().dailyRecords(user.id, from, today), windowDays);
}

AdaptiveTargetResult AdaptiveTargetService::calculate(
    const User &user, const QList<DailyFoodLogPoint> &foodPoints,
    const QList<HealthDailyRecord> &healthRecords, int windowDays)
{
    AdaptiveTargetResult result;
    result.windowDays = windowDays <= 7 ? 7 : windowDays <= 14 ? 14 : 30;
    UserService userService;
    result.baseTarget = user.calorieTarget > 0 ? user.calorieTarget
                                               : userService.calculateDailyCalories(user);
    result.effectiveTarget = result.baseTarget;

    double intakeTotal = 0.0;
    for (const DailyFoodLogPoint &point : foodPoints) {
        if (point.totals.count <= 0)
            continue;
        ++result.foodLogDays;
        intakeTotal += point.totals.calories;
    }
    if (result.foodLogDays > 0)
        result.averageIntake = intakeTotal / result.foodLogDays;

    int stepDays = 0;
    int activeDays = 0;
    int sleepDays = 0;
    int weightReadings = 0;
    double stepTotal = 0.0;
    double activeTotal = 0.0;
    double sleepTotal = 0.0;
    QDate firstWeightDate;
    QDate lastWeightDate;
    double firstWeight = 0.0;
    double lastWeight = 0.0;
    for (const HealthDailyRecord &record : healthRecords) {
        if (record.steps > 0) {
            ++stepDays;
            stepTotal += record.steps;
        }
        if (record.activeCalories > 0.0) {
            ++activeDays;
            activeTotal += record.activeCalories;
        }
        if (record.sleepHours > 0.0) {
            ++sleepDays;
            sleepTotal += record.sleepHours;
        }
        if (record.weightKg > 0.0) {
            ++weightReadings;
            if (!firstWeightDate.isValid() || record.date < firstWeightDate) {
                firstWeightDate = record.date;
                firstWeight = record.weightKg;
            }
            if (!lastWeightDate.isValid() || record.date >= lastWeightDate) {
                lastWeightDate = record.date;
                lastWeight = record.weightKg;
            }
        }
    }
    result.healthDays = healthRecords.size();
    if (stepDays > 0)
        result.averageSteps = stepTotal / stepDays;
    if (activeDays > 0)
        result.averageActiveCalories = activeTotal / activeDays;
    if (sleepDays > 0)
        result.averageSleepHours = sleepTotal / sleepDays;

    const int weightSpan = firstWeightDate.isValid() && lastWeightDate.isValid()
        ? firstWeightDate.daysTo(lastWeightDate) : 0;
    const bool weightTrendValid = weightReadings >= 2 && weightSpan >= 4;
    if (weightTrendValid) {
        result.weightChangeKg = lastWeight - firstWeight;
        result.weeklyWeightChangeKg = result.weightChangeKg * 7.0 / weightSpan;
    }

    const double foodCoverage = qMin(1.0, result.foodLogDays / qMax(4.0, result.windowDays * 0.65));
    const double healthCoverage = qMin(1.0, result.healthDays / qMax(4.0, result.windowDays * 0.65));
    const double weightCoverage = weightTrendValid ? qMin(1.0, weightReadings / 3.0) : 0.0;
    result.confidence = qBound(0.0, foodCoverage * 0.42 + healthCoverage * 0.28
                                    + weightCoverage * 0.30, 0.96);
    result.enoughData = result.foodLogDays >= 4
                        && (weightTrendValid || result.healthDays >= 4);

    if (!result.enoughData) {
        result.decision = QStringLiteral("继续收集数据");
        result.explanation = QStringLiteral(
            "当前有 %1 天完整饮食、%2 天运动/睡眠和 %3 次有效体重记录。"
            "至少记录4天饮食，并同步连续活动数据或间隔4天以上的两次体重后，才会修正每日目标；"
            "现阶段继续采用档案目标 %4 kcal。")
                                 .arg(result.foodLogDays)
                                 .arg(result.healthDays)
                                 .arg(weightReadings)
                                 .arg(result.baseTarget);
        return result;
    }

    const QString goal = user.goal.toLower();
    bool trendOnTarget = false;
    if (weightTrendValid) {
        if (goal == QLatin1String("lose"))
            trendOnTarget = result.weeklyWeightChangeKg <= -0.10
                            && result.weeklyWeightChangeKg >= -0.80;
        else if (goal == QLatin1String("gain"))
            trendOnTarget = result.weeklyWeightChangeKg >= 0.08
                            && result.weeklyWeightChangeKg <= 0.60;
        else
            trendOnTarget = qAbs(result.weeklyWeightChangeKg) <= 0.25;
    }

    double target = result.baseTarget;
    if (trendOnTarget && result.averageIntake > 0.0) {
        // 真实摄入已经产生符合目标的体重趋势时，尊重有效方案，不再机械拉回静态公式。
        target = result.averageIntake;
        result.decision = QStringLiteral("维持有效方案");
    } else if (weightTrendValid && result.averageIntake > 0.0) {
        const double inferredMaintenance = result.averageIntake
            - result.weightChangeKg * 7700.0 / qMax(1, weightSpan);
        const double goalOffset = goal == QLatin1String("lose") ? -300.0
                                  : goal == QLatin1String("gain") ? 250.0 : 0.0;
        const double observedTarget = inferredMaintenance + goalOffset;
        target = result.baseTarget * 0.30 + observedTarget * 0.70;
        result.decision = QStringLiteral("根据体重反馈修正");
    } else if (result.averageActiveCalories > 0.0) {
        const double activityAdjustment = qBound(-160.0,
            (result.averageActiveCalories - 350.0) * 0.35, 220.0);
        target = result.baseTarget + activityAdjustment;
        result.decision = QStringLiteral("根据活动消耗微调");
    } else {
        result.decision = QStringLiteral("维持档案目标");
    }

    target = qBound(result.baseTarget * 0.80, target, result.baseTarget * 1.20);
    if (result.averageSleepHours > 0.0 && result.averageSleepHours < 6.0
        && target < result.baseTarget - 100.0) {
        target = result.baseTarget - 100.0;
        result.decision += QStringLiteral("（睡眠保护）");
    }
    result.effectiveTarget = roundedTarget(target);

    QString intakePart = result.averageIntake > 0.0
        ? QStringLiteral("平均实际摄入 %1 kcal").arg(qRound(result.averageIntake))
        : QStringLiteral("实际摄入记录不足");
    QString weightPart = weightTrendValid
        ? QStringLiteral("体重%1（%2kg/周）")
              .arg(trendText(result.weightChangeKg))
              .arg(result.weeklyWeightChangeKg, 0, 'f', 2)
        : QStringLiteral("体重趋势不足");
    QString activityPart;
    if (result.averageSteps > 0.0 || result.averageActiveCalories > 0.0) {
        activityPart = QStringLiteral("；日均 %1 步、活动消耗 %2 kcal")
                           .arg(qRound(result.averageSteps))
                           .arg(qRound(result.averageActiveCalories));
    }
    QString sleepPart = result.averageSleepHours > 0.0
        ? QStringLiteral("、睡眠 %1 小时").arg(result.averageSleepHours, 0, 'f', 1) : QString();
    result.explanation = QStringLiteral(
        "过去%1天%2，%3%4%5。%6：每日目标由 %7 调整为 %8 kcal。")
                             .arg(result.windowDays)
                             .arg(intakePart)
                             .arg(weightPart)
                             .arg(activityPart)
                             .arg(sleepPart)
                             .arg(result.decision)
                             .arg(result.baseTarget)
                             .arg(result.effectiveTarget);
    return result;
}
