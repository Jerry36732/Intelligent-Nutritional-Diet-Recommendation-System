#ifndef DIETANALYTICSWIDGET_H
#define DIETANALYTICSWIDGET_H

#include "../dao/FoodLogDAO.h"
#include "../entities/HealthData.h"
#include "../entities/User.h"

#include <QWidget>

class QLabel;
class QComboBox;
class QPoint;
class QTableWidget;
class CalorieTrendChart;

class DietAnalyticsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DietAnalyticsWidget(QWidget *parent = nullptr);

    void setUser(const User &user);
    void reload();
    void setReviewState();

signals:
    void foodLogSaved(int logId);

private:
    void openFoodRecognition();
    void openHealthSync();
    void showRecentContextMenu(const QPoint &position);
    void updateView(const QList<DailyFoodLogPoint> &points,
                    const QList<FoodLogEntry> &entries);
    QString buildDeficiencyForecast(const QList<DailyFoodLogPoint> &points,
                                    const QList<FoodLogEntry> &entries) const;

    User m_user;
    CalorieTrendChart *m_chart = nullptr;
    QLabel *m_todayValue = nullptr;
    QLabel *m_averageValue = nullptr;
    QLabel *m_proteinValue = nullptr;
    QLabel *m_insights = nullptr;
    QLabel *m_analysisMeta = nullptr;
    QLabel *m_chartTitle = nullptr;
    QLabel *m_dynamicValue = nullptr;
    QLabel *m_dynamicBadge = nullptr;
    QLabel *m_dynamicExplanation = nullptr;
    QLabel *m_healthSummary = nullptr;
    QComboBox *m_window = nullptr;
    QTableWidget *m_recentTable = nullptr;
    QLabel *m_recentHint = nullptr;
    AdaptiveTargetResult m_adaptiveResult;
};

#endif // DIETANALYTICSWIDGET_H
