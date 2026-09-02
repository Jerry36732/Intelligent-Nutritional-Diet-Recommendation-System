#include "DashboardWidget.h"
#include "RecipeCard.h"
#include "UiAssets.h"

#include "../dao/RecipeDAO.h"
#include "../services/UserService.h"

#include <QFrame>
#include <QConicalGradient>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QString goalToCn(const QString &goal)
{
    const QString g = goal.toLower();
    if (g == QLatin1String("lose"))
        return QStringLiteral("减重");
    if (g == QLatin1String("gain"))
        return QStringLiteral("增肌");
    return QStringLiteral("维持");
}

class CalorieRing final : public QWidget
{
public:
    explicit CalorieRing(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedSize(140, 140);
        setProperty("progress", 0.0);
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF ring = QRectF(rect()).adjusted(10, 10, -10, -10);
        QPen base(QColor(QStringLiteral("#E9ECEB")), 10, Qt::SolidLine, Qt::FlatCap);
        p.setPen(base);
        p.drawArc(ring, 0, 360 * 16);
        QConicalGradient progressGradient(ring.center(), -90.0);
        progressGradient.setColorAt(0.00, QColor(QStringLiteral("#059669")));
        progressGradient.setColorAt(0.45, QColor(QStringLiteral("#10B981")));
        progressGradient.setColorAt(0.72, QColor(QStringLiteral("#35C98A")));
        progressGradient.setColorAt(1.00, QColor(QStringLiteral("#0891B2")));
        QPen active(QBrush(progressGradient), 10, Qt::SolidLine, Qt::RoundCap);
        p.setPen(active);
        const qreal progress = qBound<qreal>(0.0, property("progress").toDouble(), 1.0);
        p.drawArc(ring, 90 * 16, -qRound(360.0 * 16.0 * progress));
    }
};
} // namespace

DashboardWidget::DashboardWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(25, 12, 28, 9);
    root->setSpacing(7);

    auto *regenBtn = new QPushButton(QStringLiteral("重新生成方案"), this);
    regenBtn->setCursor(Qt::PointingHandCursor);
    regenBtn->setProperty("class", QVariant(QStringLiteral("PrimaryButton")));

    regenBtn->setObjectName(QStringLiteral("DashboardRegenButton"));
    UiAssets::setButtonIcon(regenBtn, QStringLiteral("regenerate"), 20, QColor(Qt::white));

    auto *overview = new QWidget(this);
    overview->setObjectName(QStringLiteral("DashboardOverview"));
    overview->setFixedHeight(159);
    auto *overviewLay = new QHBoxLayout(overview);
    overviewLay->setContentsMargins(0, 0, 0, 0);
    overviewLay->setSpacing(16);

    auto *calorieCard = new QFrame(overview);
    calorieCard->setObjectName(QStringLiteral("CalorieOverviewCard"));
    auto *calorieLay = new QHBoxLayout(calorieCard);
    calorieLay->setContentsMargins(16, 7, 18, 7);
    calorieLay->setSpacing(32);
    auto *calorieLeaves = new QLabel(calorieCard);
    calorieLeaves->setAttribute(Qt::WA_TransparentForMouseEvents);
    calorieLeaves->setFixedSize(98, 112);
    calorieLeaves->setPixmap(UiAssets::svgPixmap(
        QStringLiteral("leaf-watermark"), QSize(98, 112), QColor(), calorieLeaves));
    calorieLeaves->move(240, 47);
    calorieLeaves->lower();
    auto *ringHost = new QWidget(calorieCard);
    ringHost->setFixedSize(140, 140);
    auto *ring = new CalorieRing(ringHost);
    ring->setGeometry(0, 0, 124, 124);
    m_calorieRing = ring;
    auto *ringText = new QWidget(ringHost);
    ringText->setGeometry(0, 0, 124, 124);
    m_kcalValue = new QLabel(QStringLiteral("—"), ringText);
    m_kcalValue->setObjectName(QStringLiteral("MetricKcal"));
    m_kcalValue->setAlignment(Qt::AlignCenter);
    m_kcalValue->setGeometry(8, 35, 108, 46);
    auto *kcalUnit = new QLabel(QStringLiteral("kcal"), ringText);
    kcalUnit->setObjectName(QStringLiteral("RingUnit"));
    kcalUnit->setAlignment(Qt::AlignCenter);
    kcalUnit->setGeometry(8, 81, 108, 21);
    ringText->raise();

    auto *complete = new QVBoxLayout;
    complete->setSpacing(5);
    auto *todayLabel = new QLabel(QStringLiteral("今日方案热量"), calorieCard);
    todayLabel->setObjectName(QStringLiteral("OverviewCaption"));
    todayLabel->setToolTip(QStringLiteral(
        "这里显示早餐、午餐和晚餐中全部推荐菜品的热量之和，不代表实际摄入。"));
    auto *doneLabel = new QLabel(QStringLiteral("占每日目标"), calorieCard);
    doneLabel->setObjectName(QStringLiteral("OverviewSubCaption"));
    m_completionValue = new QLabel(QStringLiteral("0%"), calorieCard);
    m_completionValue->setObjectName(QStringLiteral("CompletionValue"));
    m_completionDetail = new QLabel(QStringLiteral("0 / 0 kcal"), calorieCard);
    m_completionDetail->setObjectName(QStringLiteral("CompletionDetail"));
    m_completionDetail->setWordWrap(true);
    complete->addStretch();
    complete->addWidget(todayLabel);
    complete->addSpacing(7);
    complete->addWidget(doneLabel);
    complete->addWidget(m_completionValue);
    complete->addWidget(m_completionDetail);
    complete->addStretch();
    calorieLay->addWidget(ringHost);
    calorieLay->addLayout(complete, 1);

    auto makeStatusCard = [&](const QString &caption, const QString &iconName,
                              const QString &tone, const QString &note, QLabel **valueOut,
                              QLabel **noteOut = nullptr) {
        auto *card = new QFrame(overview);
        card->setObjectName(QStringLiteral("DashboardStatusCard"));
        card->setProperty("tone", tone);
        auto *outer = new QVBoxLayout(card);
        outer->setContentsMargins(14, 16, 14, 12);
        outer->setSpacing(0);
        auto *lay = new QHBoxLayout;
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(10);
        auto *icon = new QLabel(card);
        icon->setObjectName(QStringLiteral("DashboardStatusIcon"));
        icon->setProperty("tone", tone);
        icon->setAlignment(Qt::AlignCenter);
        icon->setFixedSize(44, 44);
        const QColor iconColor(tone == QStringLiteral("green") ? QStringLiteral("#059669")
                                                                 : QStringLiteral("#0891B2"));
        icon->setPixmap(UiAssets::svgPixmap(iconName, QSize(24, 24), iconColor, icon));
        auto *copy = new QVBoxLayout;
        copy->setSpacing(0);
        auto *label = new QLabel(caption, card);
        label->setObjectName(QStringLiteral("DashboardStatusCaption"));
        auto *value = new QLabel(QStringLiteral("—"), card);
        value->setObjectName(QStringLiteral("DashboardStatusValue"));
        value->setProperty("tone", tone);
        copy->addWidget(label);
        copy->addWidget(value);
        lay->addWidget(icon);
        lay->addLayout(copy, 1);
        outer->addLayout(lay);
        outer->addSpacing(14);
        auto *rule = new QFrame(card);
        rule->setObjectName(QStringLiteral("DashboardStatusRule"));
        rule->setFixedHeight(1);
        outer->addWidget(rule);
        outer->addSpacing(7);
        auto *noteLabel = new QLabel(note, card);
        noteLabel->setObjectName(QStringLiteral("DashboardStatusNote"));
        noteLabel->setProperty("tone", tone);
        noteLabel->setWordWrap(true);
        noteLabel->setMinimumHeight(30);
        noteLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        outer->addWidget(noteLabel);
        outer->addStretch();
        *valueOut = value;
        if (noteOut)
            *noteOut = noteLabel;
        return card;
    };
    auto *goalCard = makeStatusCard(QStringLiteral("当前目标"), QStringLiteral("target"),
                                    QStringLiteral("green"),
                                    QStringLiteral("建议热量：— kcal/天"), &m_goalValue,
                                    &m_goalNote);
    auto *bmiCard = makeStatusCard(QStringLiteral("BMI"), QStringLiteral("bmi-user"),
                                   QStringLiteral("blue"), QStringLiteral("健康"), &m_bmiValue);
    m_bmiValue->setFont(UiAssets::bodyFont(20, QFont::DemiBold));
    // 维持 V6 的 338:184:210 比例，窗口变宽时三个卡片一起伸展，
    // 避免 BMI 单卡吸收全部剩余宽度而破坏参考图布局。
    overviewLay->addWidget(calorieCard, 338);
    overviewLay->addWidget(goalCard, 184);
    overviewLay->addWidget(bmiCard, 210);

    auto *cardsHost = new QWidget(this);
    cardsHost->setObjectName(QStringLiteral("MealCardsHost"));
    cardsHost->setFixedHeight(289);
    auto *cardsRow = new QHBoxLayout(cardsHost);
    cardsRow->setContentsMargins(0, 0, 0, 0);
    cardsRow->setSpacing(12);
    m_breakfastCard = new RecipeCard(this);
    m_lunchCard = new RecipeCard(this);
    m_dinnerCard = new RecipeCard(this);
    for (RecipeCard *card : {m_breakfastCard, m_lunchCard, m_dinnerCard})
        card->setFixedHeight(289);
    cardsRow->addWidget(m_breakfastCard, 1);
    cardsRow->addWidget(m_lunchCard, 1);
    cardsRow->addWidget(m_dinnerCard, 1);

    auto *summary = new QFrame(this);
    summary->setObjectName(QStringLiteral("DailySummaryBar"));
    summary->setFixedHeight(83);
    auto *summaryLayout = new QHBoxLayout(summary);
    summaryLayout->setContentsMargins(16, 8, 14, 8);
    summaryLayout->setSpacing(0);
    auto makeSummaryMetric = [summary](const QString &caption, const QString &tone,
                                       const QString &iconName,
                                       QLabel **valueOut) {
        auto *host = new QWidget(summary);
        host->setProperty("class", QStringLiteral("DailySummaryMetric"));
        host->setProperty("tone", tone);
        auto *layout = new QHBoxLayout(host);
        layout->setContentsMargins(10, 3, 10, 3);
        layout->setSpacing(8);
        const QColor iconColor(tone == QStringLiteral("green") ? QStringLiteral("#08A96E")
                                : tone == QStringLiteral("orange") ? QStringLiteral("#0793C7")
                                                                    : QStringLiteral("#0A8BC1"));
        auto *icon = UiAssets::createIconLabel(host, iconName, 22, iconColor);
        icon->setObjectName(QStringLiteral("DailySummaryIcon"));
        icon->setProperty("tone", tone);
        auto *copy = new QVBoxLayout;
        copy->setSpacing(2);
        auto *captionLabel = new QLabel(caption, host);
        captionLabel->setObjectName(QStringLiteral("DailySummaryCaption"));
        auto *value = new QLabel(QStringLiteral("—"), host);
        value->setObjectName(QStringLiteral("DailySummaryValue"));
        value->setProperty("tone", tone);
        copy->addWidget(captionLabel);
        copy->addWidget(value);
        layout->addWidget(icon);
        layout->addLayout(copy, 1);
        *valueOut = value;
        return host;
    };
    summaryLayout->addWidget(makeSummaryMetric(QStringLiteral("今日营养总计"),
                                                QStringLiteral("green"),
                                                QStringLiteral("leaf"),
                                                &m_totalSummaryValue), 5);
    summaryLayout->addWidget(makeSummaryMetric(QStringLiteral("蛋白质"),
                                                QStringLiteral("blue"),
                                                QStringLiteral("protein"),
                                                &m_proteinSummaryValue), 4);
    summaryLayout->addWidget(makeSummaryMetric(QStringLiteral("碳水化合物"),
                                                QStringLiteral("orange"),
                                                QStringLiteral("grain"),
                                                &m_carbsSummaryValue), 4);
    summaryLayout->addWidget(makeSummaryMetric(QStringLiteral("脂肪"),
                                                QStringLiteral("purple"),
                                                QStringLiteral("flame"),
                                                &m_fatSummaryValue), 4);
    summaryLayout->addWidget(regenBtn);
    regenBtn->setFixedSize(155, 49);

    auto *tip = new QLabel(
        QStringLiteral("膳衡原则：热量守恒、蛋白优先、三餐平衡。生成方案后可在食谱详情中收藏。"),
        this);
    tip->setObjectName(QStringLiteral("DailyRuleHint"));
    tip->setWordWrap(true);
    tip->setFixedHeight(24);

    connect(regenBtn, &QPushButton::clicked, this, &DashboardWidget::regenerateRequested);

    for (RecipeCard *card : {m_breakfastCard, m_lunchCard, m_dinnerCard}) {
        connect(card, &RecipeCard::detailClicked, this, &DashboardWidget::detailRequested);
        connect(card, &RecipeCard::mealDetailRequested, this, &DashboardWidget::mealDetailRequested);
        connect(card, &RecipeCard::favoriteToggled, this, &DashboardWidget::favoriteToggled);
    }

    root->addWidget(overview);
    root->addWidget(cardsHost);
    root->addSpacing(6);
    root->addWidget(summary);
    root->addWidget(tip);
}

void DashboardWidget::setUser(const User &user)
{
    m_user = user;
    if (m_goalNote) {
        UserService service;
        const int target = m_user.calorieTarget > 0
            ? m_user.calorieTarget : service.calculateDailyCalories(m_user);
        const int recalculated = service.calculateDailyCalories(m_user);
        m_goalNote->setText(QStringLiteral("每日目标：%1 kcal/天").arg(target));
        if (m_adaptiveTarget.enoughData) {
            m_goalNote->setText(QStringLiteral("动态目标：%1 kcal/天").arg(target));
            m_goalNote->setToolTip(m_adaptiveTarget.explanation);
        } else {
            m_goalNote->setToolTip(QStringLiteral(
                "当前采用档案保存值 %1 kcal/天。档案更新时按“基础代谢 × 活动系数1.55 × "
                "目标修正 × 医疗修正”估算；按当前资料重新计算约为 %2 kcal/天。")
                                       .arg(target)
                                       .arg(recalculated));
        }
    }
    updateMetrics();
}

void DashboardWidget::setAdaptiveTarget(const AdaptiveTargetResult &result)
{
    m_adaptiveTarget = result;
}

void DashboardWidget::setPlan(const RecommendResult &plan)
{
    m_plan = plan;
    if (plan.valid) {
        m_breakfastCard->setMeal(plan.breakfast);
        m_lunchCard->setMeal(plan.lunch);
        m_dinnerCard->setMeal(plan.dinner);
        const double breakfastKcal = plan.breakfast.totalCalories();
        const double lunchKcal = plan.lunch.totalCalories();
        const double dinnerKcal = plan.dinner.totalCalories();
        const double kcal = breakfastKcal + lunchKcal + dinnerKcal;
        const double protein = plan.breakfast.totalProtein() + plan.lunch.totalProtein()
                               + plan.dinner.totalProtein();
        const double carbs = plan.breakfast.totalCarbs() + plan.lunch.totalCarbs()
                             + plan.dinner.totalCarbs();
        const double fat = plan.breakfast.totalFat() + plan.lunch.totalFat()
                           + plan.dinner.totalFat();
        m_kcalValue->setText(QString::number(qRound(kcal)));
        m_totalSummaryValue->setText(QStringLiteral("%1 kcal").arg(kcal, 0, 'f', 0));
        m_proteinSummaryValue->setText(QStringLiteral("%1 g").arg(protein, 0, 'f', 1));
        m_carbsSummaryValue->setText(QStringLiteral("%1 g").arg(carbs, 0, 'f', 1));
        m_fatSummaryValue->setText(QStringLiteral("%1 g").arg(fat, 0, 'f', 1));
        UserService service;
        const double target = qMax(1, m_user.calorieTarget > 0
                                          ? m_user.calorieTarget
                                          : service.calculateDailyCalories(m_user));
        const int percent = qBound(0, qRound(kcal / target * 100.0), 999);
        m_completionValue->setText(QStringLiteral("%1%").arg(percent));
        m_completionDetail->setText(QStringLiteral("目标 %1 kcal").arg(qRound(target)));
        m_completionDetail->setToolTip(QStringLiteral(
            "今日方案热量 = 早餐全部菜品 + 午餐全部菜品 + 晚餐全部菜品；实际摄入请查看饮食分析。"));
        m_kcalValue->setToolTip(m_completionDetail->toolTip());
        m_calorieRing->setProperty("progress", qMin(1.0, kcal / target));
        m_calorieRing->update();
    } else {
        m_breakfastCard->clear();
        m_lunchCard->clear();
        m_dinnerCard->clear();
        m_totalSummaryValue->setText(QStringLiteral("待生成"));
        m_proteinSummaryValue->setText(QStringLiteral("—"));
        m_carbsSummaryValue->setText(QStringLiteral("—"));
        m_fatSummaryValue->setText(QStringLiteral("—"));
        m_completionValue->setText(QStringLiteral("0%"));
        UserService service;
        const int target = m_user.calorieTarget > 0
            ? m_user.calorieTarget : service.calculateDailyCalories(m_user);
        m_completionDetail->setText(QStringLiteral("尚未生成三餐方案\n目标 %1 kcal").arg(target));
        m_calorieRing->setProperty("progress", 0.0);
        m_calorieRing->update();
        updateMetrics();
    }
    if (m_user.id > 0)
        refreshFavorites(m_user.id);
}

void DashboardWidget::refreshFavorites(int userId)
{
    RecipeDAO dao;
    auto apply = [&](RecipeCard *card) {
        if (card->recipe().isValid())
            card->setFavorited(dao.isFavorite(userId, card->recipe().id));
        else
            card->setFavorited(false);
    };
    apply(m_breakfastCard);
    apply(m_lunchCard);
    apply(m_dinnerCard);
}

void DashboardWidget::updateMetrics()
{
    UserService svc;
    m_kcalValue->setObjectName(QStringLiteral("MetricKcal"));
    if (m_plan.valid) {
        const double actualKcal = m_plan.breakfast.totalCalories() + m_plan.lunch.totalCalories()
                                  + m_plan.dinner.totalCalories();
        m_kcalValue->setText(QString::number(qRound(actualKcal)));
    } else {
        m_kcalValue->setText(QStringLiteral("—"));
    }
    m_goalValue->setText(goalToCn(m_user.goal));
    m_bmiValue->setText(QString::number(svc.calculateBMI(m_user), 'f', 1));
    if (!m_plan.valid)
        m_completionValue->setText(QStringLiteral("0%"));
    if (!m_plan.valid && m_completionDetail)
        m_completionDetail->setText(QStringLiteral("尚未生成三餐方案\n目标 %1 kcal")
                                        .arg(m_user.calorieTarget > 0
                                                 ? m_user.calorieTarget
                                                 : svc.calculateDailyCalories(m_user)));
}
