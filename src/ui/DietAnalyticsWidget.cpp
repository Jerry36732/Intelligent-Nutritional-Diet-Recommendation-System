#include "DietAnalyticsWidget.h"

#include "FoodVisionDialog.h"
#include "HealthSyncDialog.h"
#include "UiAssets.h"
#include "../services/AdaptiveTargetService.h"

#include <QComboBox>
#include <QCheckBox>
#include <QCursor>
#include <QDate>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QToolTip>
#include <QVBoxLayout>
#include <QtMath>

class CalorieTrendChart : public QWidget
{
public:
    explicit CalorieTrendChart(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(188);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setData(const QList<DailyFoodLogPoint> &points, double target)
    {
        m_points = points;
        m_target = qMax(1.0, target);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRectF plot = rect().adjusted(42, 18, -16, -30);
        if (plot.width() <= 20 || plot.height() <= 20)
            return;

        double maximum = m_target * 1.25;
        bool hasData = false;
        for (const DailyFoodLogPoint &point : m_points) {
            maximum = qMax(maximum, point.totals.calories * 1.15);
            hasData = hasData || point.totals.count > 0;
        }
        maximum = qMax(500.0, maximum);

        painter.setFont(UiAssets::bodyFont(10));
        for (int i = 0; i <= 3; ++i) {
            const qreal y = plot.bottom() - plot.height() * i / 3.0;
            painter.setPen(QPen(QColor(QStringLiteral("#E2E8F0")), 1));
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
            painter.setPen(QColor(QStringLiteral("#64748B")));
            painter.drawText(QRectF(0, y - 9, plot.left() - 7, 18),
                             Qt::AlignRight | Qt::AlignVCenter,
                             QString::number(qRound(maximum * i / 3.0)));
        }

        const qreal targetY = plot.bottom() - (m_target / maximum) * plot.height();
        painter.setPen(QPen(QColor(QStringLiteral("#F59E0B")), 1.4, Qt::DashLine));
        painter.drawLine(QPointF(plot.left(), targetY), QPointF(plot.right(), targetY));
        painter.setPen(QColor(QStringLiteral("#B45309")));
        painter.drawText(QRectF(plot.right() - 76, targetY - 19, 76, 18),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QStringLiteral("目标 %1").arg(qRound(m_target)));

        if (m_points.isEmpty() || !hasData) {
            painter.setPen(QColor(QStringLiteral("#64748B")));
            painter.setFont(UiAssets::bodyFont(13));
            painter.drawText(plot, Qt::AlignCenter,
                             QStringLiteral("完成一次菜品识别并保存后，这里会生成热量趋势"));
            return;
        }

        QPainterPath line;
        QPainterPath area;
        QVector<QPointF> coordinates;
        coordinates.reserve(m_points.size());
        for (int i = 0; i < m_points.size(); ++i) {
            const qreal x = m_points.size() == 1 ? plot.center().x()
                : plot.left() + plot.width() * i / (m_points.size() - 1.0);
            const qreal y = plot.bottom()
                - qBound(0.0, m_points.at(i).totals.calories / maximum, 1.0) * plot.height();
            coordinates.append(QPointF(x, y));
            if (i == 0)
                line.moveTo(x, y);
            else
                line.lineTo(x, y);
        }
        area = line;
        area.lineTo(coordinates.last().x(), plot.bottom());
        area.lineTo(coordinates.first().x(), plot.bottom());
        area.closeSubpath();
        QLinearGradient fill(plot.topLeft(), plot.bottomLeft());
        fill.setColorAt(0.0, QColor(16, 185, 129, 62));
        fill.setColorAt(1.0, QColor(16, 185, 129, 4));
        painter.fillPath(area, fill);
        painter.setPen(QPen(QColor(QStringLiteral("#059669")), 2.4,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(line);

        const int labelStep = qMax(1, qCeil(coordinates.size() / 7.0));
        for (int i = 0; i < coordinates.size(); ++i) {
            painter.setPen(QPen(QColor(QStringLiteral("#059669")), 2));
            painter.setBrush(Qt::white);
            painter.drawEllipse(coordinates.at(i), 4.0, 4.0);
            if (i % labelStep != 0 && i != coordinates.size() - 1)
                continue;
            painter.setPen(QColor(QStringLiteral("#475569")));
            painter.setFont(UiAssets::bodyFont(9));
            painter.drawText(QRectF(coordinates.at(i).x() - 23, plot.bottom() + 7, 46, 18),
                             Qt::AlignCenter,
                             m_points.at(i).date.toString(QStringLiteral("M/d")));
        }
    }

private:
    QList<DailyFoodLogPoint> m_points;
    double m_target = 2000.0;
};

namespace {
QFrame *makeMetricCard(QWidget *parent, const QString &caption, const QString &icon,
                       const QColor &color, QLabel **valueOut)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("DietMetricCard"));
    auto *layout = new QHBoxLayout(card);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(10);
    auto *iconLabel = UiAssets::createIconLabel(card, icon, 22, color);
    iconLabel->setObjectName(QStringLiteral("DietMetricIcon"));
    iconLabel->setFixedSize(38, 38);
    auto *copy = new QVBoxLayout;
    copy->setSpacing(0);
    auto *title = new QLabel(caption, card);
    title->setObjectName(QStringLiteral("DietMetricCaption"));
    auto *value = new QLabel(QStringLiteral("—"), card);
    value->setObjectName(QStringLiteral("DietMetricValue"));
    value->setStyleSheet(QStringLiteral("color:%1;").arg(color.name()));
    copy->addWidget(title);
    copy->addWidget(value);
    layout->addWidget(iconLabel);
    layout->addLayout(copy, 1);
    *valueOut = value;
    return card;
}
}

DietAnalyticsWidget::DietAnalyticsWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("DietAnalyticsPage"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(26, 12, 26, 14);
    root->setSpacing(10);

    auto *actions = new QFrame(this);
    actions->setObjectName(QStringLiteral("DietActionBar"));
    actions->setFixedHeight(54);
    auto *actionLayout = new QHBoxLayout(actions);
    actionLayout->setContentsMargins(12, 7, 12, 7);
    actionLayout->setSpacing(9);
    auto *hint = new QLabel(QStringLiteral("拍照识别多道菜，自动估算营养并写入饮食记录"), actions);
    hint->setObjectName(QStringLiteral("DietActionHint"));
    auto *food = new QPushButton(QStringLiteral("识别菜品并记录"), actions);
    food->setObjectName(QStringLiteral("DietPrimaryAction"));
    food->setCursor(Qt::PointingHandCursor);
    food->setFixedWidth(210);
    food->setFixedHeight(40);
    UiAssets::setButtonIcon(food, QStringLiteral("camera"), 18, Qt::white);
    auto *health = new QPushButton(QStringLiteral("连接健康数据"), actions);
    health->setObjectName(QStringLiteral("DietHealthAction"));
    health->setCursor(Qt::PointingHandCursor);
    health->setFixedSize(174, 40);
    UiAssets::setButtonIcon(health, QStringLiteral("medical-heart"), 18,
                            QColor(QStringLiteral("#047857")));
    actionLayout->addWidget(hint, 1);
    actionLayout->addWidget(health);
    actionLayout->addWidget(food);
    root->addWidget(actions);

    auto *dynamic = new QFrame(this);
    dynamic->setObjectName(QStringLiteral("AdaptiveTargetCard"));
    auto *dynamicLayout = new QHBoxLayout(dynamic);
    dynamicLayout->setContentsMargins(14, 10, 14, 10);
    dynamicLayout->setSpacing(12);
    auto *adaptiveIcon = UiAssets::createIconLabel(dynamic, QStringLiteral("target"), 24,
                                                   QColor(QStringLiteral("#059669")));
    adaptiveIcon->setObjectName(QStringLiteral("AdaptiveTargetIcon"));
    adaptiveIcon->setFixedSize(42, 42);
    auto *adaptiveValueBox = new QVBoxLayout;
    adaptiveValueBox->setSpacing(0);
    auto *adaptiveCaption = new QLabel(QStringLiteral("动态每日目标"), dynamic);
    adaptiveCaption->setObjectName(QStringLiteral("AdaptiveTargetCaption"));
    m_dynamicValue = new QLabel(QStringLiteral("—"), dynamic);
    m_dynamicValue->setObjectName(QStringLiteral("AdaptiveTargetValue"));
    adaptiveValueBox->addWidget(adaptiveCaption);
    adaptiveValueBox->addWidget(m_dynamicValue);
    m_dynamicBadge = new QLabel(QStringLiteral("继续收集数据"), dynamic);
    m_dynamicBadge->setObjectName(QStringLiteral("AdaptiveTargetBadge"));
    m_dynamicBadge->setAlignment(Qt::AlignCenter);
    m_dynamicBadge->setMinimumWidth(116);
    m_dynamicBadge->setFixedHeight(30);
    auto *adaptiveCopy = new QVBoxLayout;
    adaptiveCopy->setSpacing(2);
    m_dynamicExplanation = new QLabel(dynamic);
    m_dynamicExplanation->setObjectName(QStringLiteral("AdaptiveTargetExplanation"));
    m_dynamicExplanation->setWordWrap(true);
    m_healthSummary = new QLabel(dynamic);
    m_healthSummary->setObjectName(QStringLiteral("AdaptiveHealthSummary"));
    adaptiveCopy->addWidget(m_dynamicExplanation);
    adaptiveCopy->addWidget(m_healthSummary);
    m_window = new QComboBox(dynamic);
    m_window->setObjectName(QStringLiteral("AdaptiveWindowCombo"));
    m_window->addItem(QStringLiteral("近7天"), 7);
    m_window->addItem(QStringLiteral("近14天"), 14);
    m_window->addItem(QStringLiteral("近30天"), 30);
    m_window->setCurrentIndex(1);
    m_window->setFixedSize(94, 36);
    dynamicLayout->addWidget(adaptiveIcon);
    dynamicLayout->addLayout(adaptiveValueBox);
    dynamicLayout->addWidget(m_dynamicBadge);
    dynamicLayout->addLayout(adaptiveCopy, 1);
    dynamicLayout->addWidget(m_window);
    root->addWidget(dynamic);

    auto *metrics = new QWidget(this);
    auto *metricLayout = new QHBoxLayout(metrics);
    metricLayout->setContentsMargins(0, 0, 0, 0);
    metricLayout->setSpacing(10);
    metricLayout->addWidget(makeMetricCard(metrics, QStringLiteral("今日已记录"),
                                            QStringLiteral("flame"),
                                            QColor(QStringLiteral("#F59E0B")), &m_todayValue));
    metricLayout->addWidget(makeMetricCard(metrics, QStringLiteral("窗口期平均"),
                                            QStringLiteral("chart-line"),
                                            QColor(QStringLiteral("#0891B2")), &m_averageValue));
    metricLayout->addWidget(makeMetricCard(metrics, QStringLiteral("今日蛋白质"),
                                            QStringLiteral("protein"),
                                            QColor(QStringLiteral("#059669")), &m_proteinValue));
    root->addWidget(metrics);

    auto *analysisRow = new QHBoxLayout;
    analysisRow->setSpacing(10);
    auto *chartCard = new QFrame(this);
    chartCard->setObjectName(QStringLiteral("DietPanelCard"));
    auto *chartLayout = new QVBoxLayout(chartCard);
    chartLayout->setContentsMargins(14, 10, 14, 8);
    chartLayout->setSpacing(2);
    m_chartTitle = new QLabel(QStringLiteral("近14日热量趋势"), chartCard);
    m_chartTitle->setObjectName(QStringLiteral("DietSectionTitle"));
    m_chart = new CalorieTrendChart(chartCard);
    chartLayout->addWidget(m_chartTitle);
    chartLayout->addWidget(m_chart, 1);

    auto *insightCard = new QFrame(this);
    insightCard->setObjectName(QStringLiteral("DietInsightCard"));
    insightCard->setFixedWidth(245);
    auto *insightLayout = new QVBoxLayout(insightCard);
    insightLayout->setContentsMargins(14, 12, 14, 12);
    insightLayout->setSpacing(7);
    auto *insightTitle = new QLabel(QStringLiteral("营养缺口预测"), insightCard);
    insightTitle->setObjectName(QStringLiteral("DietSectionTitle"));
    m_insights = new QLabel(insightCard);
    m_insights->setObjectName(QStringLiteral("DietInsights"));
    m_insights->setWordWrap(true);
    m_insights->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_analysisMeta = new QLabel(
        QStringLiteral("基于近期饮食记录推测，不替代化验或专业诊断。"), insightCard);
    m_analysisMeta->setObjectName(QStringLiteral("DietAnalysisMeta"));
    m_analysisMeta->setWordWrap(true);
    insightLayout->addWidget(insightTitle);
    insightLayout->addWidget(m_insights, 1);
    insightLayout->addWidget(m_analysisMeta);
    analysisRow->addWidget(chartCard, 1);
    analysisRow->addWidget(insightCard);
    root->addLayout(analysisRow, 1);

    auto *recentCard = new QFrame(this);
    recentCard->setObjectName(QStringLiteral("DietRecentCard"));
    recentCard->setFixedHeight(235);
    auto *recentLayout = new QVBoxLayout(recentCard);
    recentLayout->setContentsMargins(12, 8, 12, 8);
    recentLayout->setSpacing(5);
    auto *recentHeader = new QHBoxLayout;
    recentHeader->setSpacing(8);
    auto *recentTitle = new QLabel(QStringLiteral("最近饮食记录"), recentCard);
    recentTitle->setObjectName(QStringLiteral("DietSectionTitle"));
    m_recentHint = new QLabel(QStringLiteral("右键选择记录可删除误记内容"), recentCard);
    m_recentHint->setObjectName(QStringLiteral("DietRecentHint"));
    recentHeader->addWidget(recentTitle);
    recentHeader->addStretch();
    recentHeader->addWidget(m_recentHint);
    m_recentTable = new QTableWidget(0, 6, recentCard);
    m_recentTable->setObjectName(QStringLiteral("DietRecentTable"));
    m_recentTable->setHorizontalHeaderLabels({QStringLiteral("时间"), QStringLiteral("餐次"),
                                               QStringLiteral("食物"), QStringLiteral("份量"),
                                               QStringLiteral("热量"), QStringLiteral("蛋白质")});
    m_recentTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_recentTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_recentTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    for (int column = 3; column < 6; ++column)
        m_recentTable->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    m_recentTable->verticalHeader()->hide();
    m_recentTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_recentTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_recentTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_recentTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_recentTable->setAlternatingRowColors(true);
    m_recentTable->setShowGrid(false);
    recentLayout->addLayout(recentHeader);
    recentLayout->addWidget(m_recentTable, 1);
    root->addWidget(recentCard);

    connect(food, &QPushButton::clicked, this, &DietAnalyticsWidget::openFoodRecognition);
    connect(health, &QPushButton::clicked, this, &DietAnalyticsWidget::openHealthSync);
    connect(m_window, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { reload(); });
    connect(m_recentTable, &QTableWidget::customContextMenuRequested, this,
            &DietAnalyticsWidget::showRecentContextMenu);
}

void DietAnalyticsWidget::setUser(const User &user)
{
    m_user = user;
    reload();
}

void DietAnalyticsWidget::reload()
{
    if (m_user.id <= 0) {
        m_adaptiveResult = AdaptiveTargetService::calculate(m_user, {}, {},
            m_window ? m_window->currentData().toInt() : 14);
        updateView({}, {});
        return;
    }
    const QDate today = QDate::currentDate();
    const int windowDays = m_window ? m_window->currentData().toInt() : 14;
    m_adaptiveResult = AdaptiveTargetService().analyze(m_user, windowDays, today);
    updateView(FoodLogDAO().dailyTotals(m_user.id, today.addDays(1 - windowDays), today),
               FoodLogDAO().recentByUser(m_user.id, 30));
}

void DietAnalyticsWidget::openHealthSync()
{
    HealthSyncDialog dialog(m_user.id, this);
    connect(&dialog, &HealthSyncDialog::dataImported, this, &DietAnalyticsWidget::reload);
    dialog.exec();
    reload();
}

void DietAnalyticsWidget::openFoodRecognition()
{
    FoodVisionDialog dialog(m_user.id, this);
    connect(&dialog, &FoodVisionDialog::foodLogSaved, this, [this](int logId) {
        emit foodLogSaved(logId);
        reload();
    });
    dialog.exec();
    reload();
}

void DietAnalyticsWidget::showRecentContextMenu(const QPoint &position)
{
    const int row = m_recentTable->rowAt(position.y());
    if (row < 0)
        return;
    QTableWidgetItem *nameItem = m_recentTable->item(row, 2);
    if (!nameItem)
        return;
    const int logId = nameItem->data(Qt::UserRole).toInt();
    if (logId <= 0)
        return;

    m_recentTable->selectRow(row);
    QMenu menu(this);
    QAction *removeAction = menu.addAction(QStringLiteral("删除这条饮食记录"));
    QAction *selected = menu.exec(m_recentTable->viewport()->mapToGlobal(position));
    if (selected != removeAction)
        return;

    const QString foodName = nameItem->text();
    const QString recordedAt = m_recentTable->item(row, 0)
        ? m_recentTable->item(row, 0)->text() : QString();
    QSettings settings;
    const QString skipConfirmationKey = QStringLiteral(
        "diet/users/%1/skipFoodLogDeleteConfirmation").arg(m_user.id);
    if (!settings.value(skipConfirmationKey, false).toBool()) {
        QMessageBox confirmation(this);
        confirmation.setWindowTitle(QStringLiteral("确认删除饮食记录"));
        confirmation.setIcon(QMessageBox::Warning);
        confirmation.setText(QStringLiteral("确定删除“%1”（%2）吗？").arg(foodName, recordedAt));
        confirmation.setInformativeText(
            QStringLiteral("删除后会同步更新今日热量、趋势和动态目标。"));
        auto *skipPrompt = new QCheckBox(QStringLiteral("以后不再提醒"), &confirmation);
        skipPrompt->setObjectName(QStringLiteral("DietSkipDeletePrompt"));
        skipPrompt->setToolTip(QStringLiteral("勾选后，右键选择删除将直接执行。"));
        confirmation.setCheckBox(skipPrompt);
        QPushButton *removeButton = confirmation.addButton(
            QStringLiteral("删除记录"), QMessageBox::DestructiveRole);
        removeButton->setObjectName(QStringLiteral("DietDeleteConfirm"));
        QPushButton *cancelButton = confirmation.addButton(
            QStringLiteral("取消"), QMessageBox::RejectRole);
        confirmation.setDefaultButton(cancelButton);
        confirmation.exec();
        if (confirmation.clickedButton() != removeButton)
            return;
        if (skipPrompt->isChecked()) {
            settings.setValue(skipConfirmationKey, true);
            settings.sync();
        }
    }

    QString error;
    if (!FoodLogDAO().remove(logId, m_user.id, &error)) {
        QMessageBox::warning(this, QStringLiteral("删除失败"), error);
        return;
    }
    reload();
    QToolTip::showText(QCursor::pos(), QStringLiteral("已删除“%1”，统计已更新").arg(foodName),
                       m_recentTable, {}, 2500);
}

void DietAnalyticsWidget::updateView(const QList<DailyFoodLogPoint> &points,
                                     const QList<FoodLogEntry> &entries)
{
    const double target = qMax(1, m_adaptiveResult.effectiveTarget > 0
        ? m_adaptiveResult.effectiveTarget : m_user.calorieTarget);
    m_chart->setData(points, target);
    if (m_chartTitle)
        m_chartTitle->setText(QStringLiteral("近%1日热量趋势").arg(m_adaptiveResult.windowDays));
    if (m_dynamicValue) {
        const int base = m_adaptiveResult.baseTarget > 0
            ? m_adaptiveResult.baseTarget : m_user.calorieTarget;
        m_dynamicValue->setText(base == qRound(target)
            ? QStringLiteral("%1 kcal").arg(qRound(target))
            : QStringLiteral("%1 → %2 kcal").arg(base).arg(qRound(target)));
        m_dynamicBadge->setText(m_adaptiveResult.enoughData
            ? QStringLiteral("%1天 · 置信%2%")
                  .arg(m_adaptiveResult.windowDays)
                  .arg(qRound(m_adaptiveResult.confidence * 100.0))
            : QStringLiteral("继续收集数据"));
        m_dynamicBadge->setProperty("active", m_adaptiveResult.enoughData);
        m_dynamicBadge->style()->unpolish(m_dynamicBadge);
        m_dynamicBadge->style()->polish(m_dynamicBadge);
        m_dynamicExplanation->setText(m_adaptiveResult.explanation);
        m_healthSummary->setText(
            m_adaptiveResult.healthDays > 0
                ? QStringLiteral("日均 %1 步 · 活动 %2 kcal · 睡眠 %3 h")
                      .arg(qRound(m_adaptiveResult.averageSteps))
                      .arg(qRound(m_adaptiveResult.averageActiveCalories))
                      .arg(m_adaptiveResult.averageSleepHours, 0, 'f', 1)
                : QStringLiteral("尚未同步运动、体重与睡眠数据"));
    }
    const DailyFoodLogTotals today = points.isEmpty() ? DailyFoodLogTotals{}
                                                       : points.last().totals;
    int activeDays = 0;
    double totalCalories = 0.0;
    for (const DailyFoodLogPoint &point : points) {
        if (point.totals.count > 0) {
            ++activeDays;
            totalCalories += point.totals.calories;
        }
    }
    m_todayValue->setText(QStringLiteral("%1 / %2 kcal")
                              .arg(qRound(today.calories)).arg(qRound(target)));
    m_averageValue->setText(activeDays > 0
        ? QStringLiteral("%1 kcal").arg(qRound(totalCalories / activeDays))
        : QStringLiteral("暂无记录"));
    m_proteinValue->setText(QStringLiteral("%1 g").arg(today.protein, 0, 'f', 1));
    m_insights->setText(buildDeficiencyForecast(points, entries));

    m_recentTable->setRowCount(qMin(8, entries.size()));
    for (int row = 0; row < m_recentTable->rowCount(); ++row) {
        const FoodLogEntry &entry = entries.at(row);
        const QStringList values = {
            entry.eatenAt.toString(QStringLiteral("M/d HH:mm")), entry.mealLabel,
            entry.foodName, QStringLiteral("%1 g").arg(qRound(entry.servingGrams)),
            QStringLiteral("%1 kcal").arg(qRound(entry.calories)),
            QStringLiteral("%1 g").arg(entry.protein, 0, 'f', 1)};
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            item->setData(Qt::UserRole, entry.id);
            if (column >= 3)
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_recentTable->setItem(row, column, item);
        }
        m_recentTable->setRowHeight(row, 30);
    }
}

QString DietAnalyticsWidget::buildDeficiencyForecast(
    const QList<DailyFoodLogPoint> &points, const QList<FoodLogEntry> &entries) const
{
    int activeDays = 0;
    double protein = 0.0;
    double calories = 0.0;
    double fat = 0.0;
    for (const DailyFoodLogPoint &point : points) {
        if (point.totals.count <= 0)
            continue;
        ++activeDays;
        protein += point.totals.protein;
        calories += point.totals.calories;
        fat += point.totals.fat;
    }
    if (activeDays < 2)
        return QStringLiteral("记录至少2天后，可开始分析蛋白质、蔬果、钙和铁的摄入趋势。\n\n现在可先拍照识别一餐，系统会自动拆分并记录每道菜。");

    QString allFoods;
    const QDate analysisFrom = points.isEmpty() ? QDate::currentDate().addDays(-6)
                                                 : points.first().date;
    for (const FoodLogEntry &entry : entries) {
        if (!entry.eatenAt.isValid() || entry.eatenAt.date() >= analysisFrom)
            allFoods += entry.foodName + QLatin1Char(' ');
    }
    const double avgProtein = protein / activeDays;
    const double proteinFactor = m_user.goal == QLatin1String("gain") ? 1.8
                                 : m_user.goal == QLatin1String("lose") ? 1.2 : 1.5;
    const double proteinTarget = (m_user.weight > 0 ? m_user.weight : 60.0)
                                 * proteinFactor;
    QStringList risks;
    if (avgProtein < proteinTarget * 0.75)
        risks << QStringLiteral("• 蛋白质可能不足：近期日均 %1g，建议增加鱼、蛋、奶或豆制品。")
                     .arg(avgProtein, 0, 'f', 0);
    const QStringList vegetables = {QStringLiteral("菜"), QStringLiteral("瓜"),
                                    QStringLiteral("番茄"), QStringLiteral("菌"),
                                    QStringLiteral("果"), QStringLiteral("橙"),
                                    QStringLiteral("菠菜"), QStringLiteral("西兰花")};
    bool hasVegetable = false;
    for (const QString &word : vegetables)
        hasVegetable = hasVegetable || allFoods.contains(word);
    if (!hasVegetable)
        risks << QStringLiteral("• 膳食纤维与维生素C可能偏少：近期记录中蔬果出现较少。");
    if (!allFoods.contains(QStringLiteral("奶")) && !allFoods.contains(QStringLiteral("豆腐"))
        && !allFoods.contains(QStringLiteral("芝麻")))
        risks << QStringLiteral("• 钙摄入可能不足：可加入奶类、豆腐或芝麻类食物。");
    if (!allFoods.contains(QStringLiteral("牛肉")) && !allFoods.contains(QStringLiteral("肝"))
        && !allFoods.contains(QStringLiteral("菠菜")) && !allFoods.contains(QStringLiteral("贝")))
        risks << QStringLiteral("• 铁来源可能不足：可关注瘦红肉、动物肝或深绿色蔬菜。");
    if (calories > 0.0 && fat * 9.0 / calories > 0.38)
        risks << QStringLiteral("• 脂肪供能占比较高：建议减少油炸和额外烹调油。");
    if (risks.isEmpty())
        risks << QStringLiteral("• 近期宏量营养结构较均衡，继续保持多样化记录。");
    return risks.mid(0, 3).join(QLatin1Char('\n'));
}

void DietAnalyticsWidget::setReviewState()
{
    if (m_user.id <= 0) {
        m_user.id = 1;
        m_user.weight = 70.0;
        m_user.calorieTarget = 2100;
    }
    m_user.calorieTarget = 2100;
    m_user.goal = QStringLiteral("lose");
    const QDate today = QDate::currentDate();
    if (m_window) {
        const QSignalBlocker blocker(m_window);
        m_window->setCurrentIndex(1);
    }
    const QList<double> calories = {2180, 2310, 2260, 2230, 2290, 2200, 2250,
                                    2340, 2210, 2280, 2190, 2270, 2260, 2230};
    QList<DailyFoodLogPoint> points;
    for (int i = 0; i < calories.size(); ++i) {
        DailyFoodLogTotals totals;
        totals.count = 3;
        totals.calories = calories.at(i);
        totals.protein = 82.0 + i * 3.0;
        totals.carbs = 240.0;
        totals.fat = 66.0;
        points.append({today.addDays(i - 13), totals});
    }
    QList<HealthDailyRecord> health;
    for (int i = 0; i < 14; ++i) {
        HealthDailyRecord record;
        record.date = today.addDays(i - 13);
        record.steps = 7800 + (i % 5) * 310;
        record.activeCalories = 395 + (i % 4) * 24;
        record.sleepHours = 6.8 + (i % 3) * 0.3;
        if (i == 0)
            record.weightKg = 70.4;
        else if (i == 13)
            record.weightKg = 70.0;
        record.source = QStringLiteral("apple-healthkit");
        health.append(record);
    }
    m_adaptiveResult = AdaptiveTargetService::calculate(m_user, points, health, 14);
    QList<FoodLogEntry> entries;
    for (int i = 0; i < 5; ++i) {
        FoodLogEntry entry;
        entry.eatenAt = QDateTime::currentDateTime().addSecs(-i * 3600);
        entry.mealLabel = i < 2 ? QStringLiteral("午餐") : QStringLiteral("早餐");
        entry.foodName = QStringList{QStringLiteral("清蒸鲈鱼"), QStringLiteral("白米饭"),
                                     QStringLiteral("番茄炒蛋"), QStringLiteral("牛奶"),
                                     QStringLiteral("燕麦粥")}.at(i);
        entry.servingGrams = 180 + i * 20;
        entry.calories = 280 + i * 35;
        entry.protein = 18 + i * 2;
        entries.append(entry);
    }
    updateView(points, entries);
}
