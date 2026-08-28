#include "DashboardWidget.h"
#include "RecipeCard.h"

#include "../dao/RecipeDAO.h"
#include "../services/UserService.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
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
} // namespace

DashboardWidget::DashboardWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(16);

    auto *topRow = new QHBoxLayout;
    auto *heroCol = new QVBoxLayout;
    heroCol->setSpacing(8);
    auto *eyebrow = new QLabel(QStringLiteral("DAILY PLAN · 今日方案"), this);
    eyebrow->setObjectName(QStringLiteral("SectionKicker"));
    m_welcomeLabel = new QLabel(QStringLiteral("把今天的每一口，\n吃成靠近目标的样子。"), this);
    m_welcomeLabel->setObjectName(QStringLiteral("DashboardHeroTitle"));
    m_welcomeLabel->setWordWrap(true);
    auto *sub = new QLabel(QStringLiteral("根据你的身体数据与目标，系统已准备好今日三餐。"), this);
    sub->setObjectName(QStringLiteral("DashboardSubcopy"));
    heroCol->addWidget(eyebrow);
    heroCol->addWidget(m_welcomeLabel);
    heroCol->addWidget(sub);

    auto *regenBtn = new QPushButton(QStringLiteral("重新生成方案"), this);
    regenBtn->setCursor(Qt::PointingHandCursor);
    regenBtn->setProperty("class", QVariant(QStringLiteral("PrimaryButton")));

    auto *settingsBtn = new QPushButton(QStringLiteral("调整目标"), this);
    settingsBtn->setCursor(Qt::PointingHandCursor);
    settingsBtn->setProperty("class", QVariant(QStringLiteral("GhostButton")));

    topRow->addLayout(heroCol, 1);
    topRow->addStretch();
    auto *btnCol = new QVBoxLayout;
    btnCol->addStretch();
    btnCol->addWidget(settingsBtn);
    btnCol->addWidget(regenBtn);
    topRow->addLayout(btnCol);

    auto *metrics = new QFrame(this);
    metrics->setObjectName(QStringLiteral("MetricStrip"));
    auto *metricsLay = new QHBoxLayout(metrics);
    metricsLay->setContentsMargins(20, 16, 20, 16);
    metricsLay->setSpacing(0);

    auto addMetric = [&](QLabel **valueOut, const QString &label) {
        auto *box = new QVBoxLayout;
        box->setSpacing(4);
        auto *v = new QLabel(QStringLiteral("—"), metrics);
        v->setProperty("class", QVariant(QStringLiteral("MetricValue")));
        v->setAlignment(Qt::AlignCenter);
        auto *l = new QLabel(label, metrics);
        l->setProperty("class", QVariant(QStringLiteral("MetricLabel")));
        l->setAlignment(Qt::AlignCenter);
        box->addWidget(v);
        box->addWidget(l);
        metricsLay->addLayout(box, 1);
        *valueOut = v;

        auto *div = new QFrame(metrics);
        div->setProperty("class", QVariant(QStringLiteral("MetricDivider")));
        div->setFrameShape(QFrame::VLine);
        metricsLay->addWidget(div);
    };

    addMetric(&m_kcalValue, QStringLiteral("热量目标 kcal"));
    addMetric(&m_goalValue, QStringLiteral("饮食目标"));
    addMetric(&m_bmiValue, QStringLiteral("BMI"));
    addMetric(&m_completionValue, QStringLiteral("今日完成度"));
    // remove last divider
    if (QLayoutItem *last = metricsLay->takeAt(metricsLay->count() - 1)) {
        if (QWidget *w = last->widget())
            w->deleteLater();
        delete last;
    }

    auto *cardsRow = new QHBoxLayout;
    cardsRow->setSpacing(14);
    m_breakfastCard = new RecipeCard(this);
    m_lunchCard = new RecipeCard(this);
    m_dinnerCard = new RecipeCard(this);
    cardsRow->addWidget(m_breakfastCard, 1);
    cardsRow->addWidget(m_lunchCard, 1);
    cardsRow->addWidget(m_dinnerCard, 1);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setProperty("class", QVariant(QStringLiteral("HintText")));

    auto *tip = new QFrame(this);
    tip->setObjectName(QStringLiteral("RuleCard"));
    auto *tipLay = new QHBoxLayout(tip);
    tipLay->setContentsMargins(14, 12, 14, 12);
    auto *tipLetter = new QLabel(QStringLiteral("衡"), tip);
    tipLetter->setProperty("class", QVariant(QStringLiteral("RuleLetter")));
    auto *tipText = new QLabel(
        QStringLiteral("膳衡原则：热量守恒、蛋白优先、三餐平衡。生成方案后可收藏并在档案页回看。"),
        tip);
    tipText->setWordWrap(true);
    tipText->setProperty("class", QVariant(QStringLiteral("HintText")));
    tipLay->addWidget(tipLetter, 0, Qt::AlignTop);
    tipLay->addWidget(tipText, 1);

    connect(regenBtn, &QPushButton::clicked, this, &DashboardWidget::regenerateRequested);
    connect(settingsBtn, &QPushButton::clicked, this, &DashboardWidget::openSettingsRequested);

    for (RecipeCard *card : {m_breakfastCard, m_lunchCard, m_dinnerCard}) {
        connect(card, &RecipeCard::detailClicked, this, &DashboardWidget::detailRequested);
        connect(card, &RecipeCard::mealDetailRequested, this, &DashboardWidget::mealDetailRequested);
        connect(card, &RecipeCard::favoriteToggled, this, &DashboardWidget::favoriteToggled);
    }

    root->addLayout(topRow);
    root->addWidget(metrics);
    root->addLayout(cardsRow, 1);
    root->addWidget(m_summaryLabel);
    root->addWidget(tip);
}

void DashboardWidget::setUser(const User &user)
{
    m_user = user;
    m_welcomeLabel->setText(QStringLiteral("把今天的每一口，\n吃成靠近目标的样子。"));
    if (QLabel *sub = findChild<QLabel *>(QStringLiteral("DashboardSubcopy"))) {
        sub->setText(QStringLiteral("早安，%1。根据你的身体数据，今天适合保持稳定的能量摄入。")
                         .arg(user.name.isEmpty() ? QStringLiteral("朋友") : user.name));
    }
    updateMetrics();
}

void DashboardWidget::setPlan(const RecommendResult &plan)
{
    m_plan = plan;
    if (plan.valid) {
        m_breakfastCard->setMeal(plan.breakfast);
        m_lunchCard->setMeal(plan.lunch);
        m_dinnerCard->setMeal(plan.dinner);
        m_summaryLabel->setText(plan.summary);
        m_completionValue->setText(QStringLiteral("100%"));
    } else {
        m_breakfastCard->clear();
        m_lunchCard->clear();
        m_dinnerCard->clear();
        m_summaryLabel->setText(plan.summary.isEmpty()
                                    ? QStringLiteral("尚未生成方案，点击「重新生成方案」开始。")
                                    : plan.summary);
        m_completionValue->setText(QStringLiteral("0%"));
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
    m_kcalValue->setText(QString::number(m_user.calorieTarget > 0
                                             ? m_user.calorieTarget
                                             : svc.calculateDailyCalories(m_user)));
    m_goalValue->setText(goalToCn(m_user.goal));
    m_bmiValue->setText(QString::number(svc.calculateBMI(m_user), 'f', 1));
    if (!m_plan.valid)
        m_completionValue->setText(QStringLiteral("0%"));
}
