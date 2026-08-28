#include "ProfileWidget.h"
#include "RecipeCard.h"

#include "../dao/RecipeDAO.h"
#include "../services/UserService.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
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

QFrame *makeStatCard(QWidget *parent, const QString &label, QLabel **value, QLabel **note)
{
    auto *card = new QFrame(parent);
    card->setProperty("class", QVariant(QStringLiteral("ProfileStatCard")));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 13, 16, 13);
    layout->setSpacing(3);
    auto *caption = new QLabel(label, card);
    caption->setProperty("class", QVariant(QStringLiteral("MetricLabel")));
    *value = new QLabel(QStringLiteral("—"), card);
    (*value)->setProperty("class", QVariant(QStringLiteral("ProfileStatValue")));
    *note = new QLabel(card);
    (*note)->setProperty("class", QVariant(QStringLiteral("ProfileStatNote")));
    layout->addWidget(caption);
    layout->addWidget(*value);
    layout->addWidget(*note);
    return card;
}
} // namespace

ProfileWidget::ProfileWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(34, 28, 34, 26);
    root->setSpacing(16);

    auto *hero = new QFrame(this);
    hero->setObjectName(QStringLiteral("ProfileHero"));
    auto *heroLayout = new QHBoxLayout(hero);
    heroLayout->setContentsMargins(23, 19, 23, 19);
    heroLayout->setSpacing(14);

    auto *avatar = new QLabel(QStringLiteral("档"), hero);
    avatar->setObjectName(QStringLiteral("ProfileAvatar"));
    avatar->setAlignment(Qt::AlignCenter);
    auto *intro = new QVBoxLayout;
    intro->setSpacing(4);
    auto *eyebrow = new QLabel(QStringLiteral("LOCAL NUTRITION PROFILE"), hero);
    eyebrow->setObjectName(QStringLiteral("ProfileEyebrow"));
    m_nameLabel = new QLabel(QStringLiteral("我的营养档案"), hero);
    m_nameLabel->setObjectName(QStringLiteral("ProfileName"));
    auto *copy = new QLabel(QStringLiteral("多维健康档案支持饮食选择、不耐受、营养缺乏、过敏与医疗状况。"), hero);
    copy->setWordWrap(true);
    copy->setProperty("class", QVariant(QStringLiteral("HintText")));
    intro->addWidget(eyebrow);
    intro->addWidget(m_nameLabel);
    intro->addWidget(copy);

    auto *settingsBtn = new QPushButton(QStringLiteral("编辑档案"), hero);
    settingsBtn->setCursor(Qt::PointingHandCursor);
    settingsBtn->setProperty("class", QVariant(QStringLiteral("GhostButton")));
    heroLayout->addWidget(avatar);
    heroLayout->addLayout(intro);
    heroLayout->addStretch();
    heroLayout->addWidget(settingsBtn);

    auto *stats = new QFrame(this);
    stats->setObjectName(QStringLiteral("ProfileStats"));
    auto *statsLayout = new QHBoxLayout(stats);
    statsLayout->setContentsMargins(1, 1, 1, 1);
    statsLayout->setSpacing(0);
    QLabel *heightNote = nullptr, *weightNote = nullptr;
    statsLayout->addWidget(makeStatCard(stats, QStringLiteral("身高"), &m_heightValue, &heightNote), 1);
    statsLayout->addWidget(makeStatCard(stats, QStringLiteral("体重"), &m_weightValue, &weightNote), 1);
    statsLayout->addWidget(makeStatCard(stats, QStringLiteral("BMI 指数"), &m_bmiValue, &m_bmiNote), 1);
    statsLayout->addWidget(makeStatCard(stats, QStringLiteral("当前目标"), &m_goalValue, &m_goalNote), 1);

    auto *healthCard = new QFrame(this);
    healthCard->setObjectName(QStringLiteral("ProfileHealthCard"));
    healthCard->setProperty("class", QVariant(QStringLiteral("PanelCard")));
    auto *healthOuter = new QVBoxLayout(healthCard);
    healthOuter->setContentsMargins(18, 14, 18, 14);
    healthOuter->setSpacing(10);
    auto *healthTitle = new QLabel(QStringLiteral("多维健康档案"), healthCard);
    healthTitle->setProperty("class", QVariant(QStringLiteral("SectionTitle")));
    auto *healthHint = new QLabel(QStringLiteral("点击「编辑档案」可多选标签更新；推荐引擎将据此避雷与加权。"),
                                  healthCard);
    healthHint->setWordWrap(true);
    healthHint->setProperty("class", QVariant(QStringLiteral("HintText")));
    m_healthLayout = new QVBoxLayout;
    m_healthLayout->setSpacing(8);
    healthOuter->addWidget(healthTitle);
    healthOuter->addWidget(healthHint);
    healthOuter->addLayout(m_healthLayout);

    auto *favoritesTitleRow = new QHBoxLayout;
    auto *favoritesBox = new QVBoxLayout;
    favoritesBox->setSpacing(3);
    auto *favoritesEyebrow = new QLabel(QStringLiteral("SAVED RECIPES"), this);
    favoritesEyebrow->setObjectName(QStringLiteral("FoodEyebrow"));
    auto *favoritesTitle = new QLabel(QStringLiteral("我的食谱收藏"), this);
    favoritesTitle->setProperty("class", QVariant(QStringLiteral("SectionTitle")));
    auto *favoritesHint = new QLabel(QStringLiteral("收藏的食谱保存在本地个人档案中。"), this);
    favoritesHint->setProperty("class", QVariant(QStringLiteral("HintText")));
    favoritesBox->addWidget(favoritesEyebrow);
    favoritesBox->addWidget(favoritesTitle);
    favoritesBox->addWidget(favoritesHint);
    m_favCount = new QLabel(QStringLiteral("0 已收藏"), this);
    m_favCount->setObjectName(QStringLiteral("FavoriteBadge"));
    favoritesTitleRow->addLayout(favoritesBox);
    favoritesTitleRow->addStretch();
    favoritesTitleRow->addWidget(m_favCount, 0, Qt::AlignBottom);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *favHost = new QWidget;
    m_favLayout = new QVBoxLayout(favHost);
    m_favLayout->setContentsMargins(0, 0, 0, 0);
    m_favLayout->setSpacing(12);
    m_favLayout->addStretch();
    scroll->setWidget(favHost);

    connect(settingsBtn, &QPushButton::clicked, this, &ProfileWidget::openSettingsRequested);
    root->addWidget(hero);
    root->addWidget(stats);
    root->addWidget(healthCard);
    root->addLayout(favoritesTitleRow);
    root->addWidget(scroll, 1);
}

QWidget *ProfileWidget::makeDimensionRow(const QString &title, const QStringList &values)
{
    auto *row = new QWidget(this);
    auto *lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    auto *t = new QLabel(title, row);
    t->setObjectName(QStringLiteral("HealthDimTitle"));
    t->setFixedWidth(88);
    lay->addWidget(t, 0, Qt::AlignTop);

    auto *chips = new QWidget(row);
    auto *chipLay = new QGridLayout(chips);
    chipLay->setContentsMargins(0, 0, 0, 0);
    chipLay->setHorizontalSpacing(6);
    chipLay->setVerticalSpacing(6);
    if (values.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("未设置"), chips);
        empty->setProperty("class", QVariant(QStringLiteral("HintText")));
        chipLay->addWidget(empty, 0, 0);
    } else {
        int col = 0;
        int r = 0;
        for (const QString &v : values) {
            auto *chip = new QLabel(v, chips);
            chip->setObjectName(QStringLiteral("HealthChip"));
            chip->setAlignment(Qt::AlignCenter);
            chip->setMinimumHeight(28);
            chip->setTextInteractionFlags(Qt::TextSelectableByMouse);
            chipLay->addWidget(chip, r, col);
            ++col;
            if (col >= 4) {
                col = 0;
                ++r;
            }
        }
    }
    lay->addWidget(chips, 1);
    return row;
}

void ProfileWidget::refreshHealthSummary()
{
    if (!m_healthLayout)
        return;
    while (QLayoutItem *item = m_healthLayout->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete item;
    }

    m_healthLayout->addWidget(makeDimensionRow(QStringLiteral("饮食选择"), m_user.dietaryChoices));
    m_healthLayout->addWidget(makeDimensionRow(QStringLiteral("食物不耐受"), m_user.foodIntolerances));
    m_healthLayout->addWidget(makeDimensionRow(QStringLiteral("营养缺乏"), m_user.nutritionalDeficiencies));
    m_healthLayout->addWidget(makeDimensionRow(QStringLiteral("过敏史"),
                                               m_user.allergies.isEmpty()
                                                   ? User::splitLegacyText(m_user.allergens)
                                                   : m_user.allergies));
    m_healthLayout->addWidget(makeDimensionRow(QStringLiteral("医疗状况"), m_user.medicalConditions));
}

void ProfileWidget::setUser(const User &user)
{
    m_user = user;
    UserService svc;
    const double bmi = svc.calculateBMI(user);
    m_nameLabel->setText(user.name.isEmpty() ? QStringLiteral("我的营养档案")
                                             : QStringLiteral("%1的营养档案").arg(user.name));
    m_heightValue->setText(QStringLiteral("%1 cm").arg(user.height, 0, 'f', 0));
    m_weightValue->setText(QStringLiteral("%1 kg").arg(user.weight, 0, 'f', 1));
    m_bmiValue->setText(QString::number(bmi, 'f', 1));
    m_bmiNote->setText(bmi >= 18.5 && bmi < 24.0 ? QStringLiteral("健康范围") : QStringLiteral("注意身体状态"));
    m_goalValue->setText(goalToCn(user.goal));
    m_goalNote->setText(QStringLiteral("%1 kcal / 日").arg(user.calorieTarget));
    refreshHealthSummary();
}

void ProfileWidget::clearFavoriteCards()
{
    if (!m_favLayout)
        return;
    while (QLayoutItem *item = m_favLayout->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete item;
    }
}

void ProfileWidget::reloadFavorites()
{
    clearFavoriteCards();
    if (m_user.id <= 0) {
        auto *empty = new QLabel(QStringLiteral("登录后可收藏喜欢的食谱。"), this);
        empty->setObjectName(QStringLiteral("FavoriteEmpty"));
        empty->setAlignment(Qt::AlignCenter);
        m_favLayout->addWidget(empty);
        m_favLayout->addStretch();
        m_favCount->setText(QStringLiteral("0 已收藏"));
        return;
    }
    RecipeDAO dao;
    const QList<Recipe> favorites = dao.findFavorites(m_user.id);
    m_favCount->setText(QStringLiteral("♥  %1 已收藏").arg(favorites.size()));
    if (favorites.isEmpty()) {
        auto *empty = new QLabel(
            QStringLiteral("还没有收藏食谱\n在今日方案中点击爱心，即可把喜欢的食谱保存到这里。"), this);
        empty->setObjectName(QStringLiteral("FavoriteEmpty"));
        empty->setAlignment(Qt::AlignCenter);
        m_favLayout->addWidget(empty);
        m_favLayout->addStretch();
        return;
    }
    for (const Recipe &recipe : favorites) {
        auto *card = new RecipeCard(this);
        card->setRecipe(recipe);
        card->setFavorited(true);
        connect(card, &RecipeCard::detailClicked, this, &ProfileWidget::detailRequested);
        connect(card, &RecipeCard::favoriteToggled, this, &ProfileWidget::favoriteToggled);
        m_favLayout->addWidget(card);
    }
    m_favLayout->addStretch();
}
