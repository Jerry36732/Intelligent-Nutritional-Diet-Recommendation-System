#include "ProfileWidget.h"
#include "UiAssets.h"

#include "../dao/FoodDAO.h"
#include "../dao/RecipeDAO.h"
#include "../services/UserService.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QString goalToCn(const QString &goal)
{
    if (goal.compare(QStringLiteral("lose"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("减重计划");
    if (goal.compare(QStringLiteral("gain"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("增肌计划");
    return QStringLiteral("体重维持");
}

QColor toneColor(const QString &tone)
{
    if (tone == QStringLiteral("orange")) return QColor(QStringLiteral("#E99A2E"));
    if (tone == QStringLiteral("blue")) return QColor(QStringLiteral("#5577D3"));
    if (tone == QStringLiteral("purple")) return QColor(QStringLiteral("#765FD0"));
    return QColor(QStringLiteral("#08A96E"));
}

QWidget *makeValueBlock(QWidget *parent, const QString &caption, const QString &iconName,
                        const QString &tone, QLabel **value)
{
    auto *block = new QFrame(parent);
    block->setProperty("class", QStringLiteral("ProfileValueBlock"));
    block->setProperty("tone", tone);
    auto *lay = new QHBoxLayout(block);
    lay->setContentsMargins(12, 9, 12, 9);
    lay->setSpacing(10);
    auto *icon = new QLabel(block);
    icon->setObjectName(QStringLiteral("ProfileValueIcon"));
    icon->setProperty("tone", tone);
    icon->setAlignment(Qt::AlignCenter);
    icon->setFixedSize(42, 42);
    icon->setPixmap(UiAssets::svgPixmap(iconName, QSize(24, 24), toneColor(tone), icon));
    auto *text = new QVBoxLayout;
    text->setSpacing(3);
    auto *label = new QLabel(caption, block);
    label->setObjectName(QStringLiteral("ProfileValueCaption"));
    *value = new QLabel(QStringLiteral("—"), block);
    (*value)->setObjectName(QStringLiteral("ProfileValueText"));
    text->addWidget(label);
    text->addWidget(*value);
    lay->addWidget(icon);
    lay->addLayout(text, 1);
    return block;
}

QWidget *makeHealthBlock(QWidget *parent, const QString &caption, const QString &iconName,
                         const QString &tone, QLabel **value)
{
    auto *block = new QFrame(parent);
    block->setProperty("class", QStringLiteral("HealthValueBlock"));
    block->setProperty("tone", tone);
    auto *lay = new QHBoxLayout(block);
    lay->setContentsMargins(11, 8, 11, 8);
    lay->setSpacing(9);
    auto *icon = new QLabel(block);
    icon->setObjectName(QStringLiteral("HealthValueIcon"));
    icon->setProperty("tone", tone);
    icon->setAlignment(Qt::AlignCenter);
    icon->setFixedSize(40, 40);
    icon->setPixmap(UiAssets::svgPixmap(iconName, QSize(22, 22), toneColor(tone), icon));
    auto *text = new QVBoxLayout;
    text->setSpacing(3);
    auto *label = new QLabel(caption, block);
    label->setObjectName(QStringLiteral("HealthValueCaption"));
    *value = new QLabel(QStringLiteral("暂无记录"), block);
    (*value)->setObjectName(QStringLiteral("HealthValueText"));
    (*value)->setWordWrap(true);
    text->addWidget(label);
    text->addWidget(*value);
    lay->addWidget(icon);
    lay->addLayout(text, 1);
    return block;
}

QWidget *makeFavoriteBlock(QWidget *parent, const QString &caption, const QString &iconName,
                           const QString &tone, QLabel **value, QPushButton **openButton)
{
    auto *block = new QFrame(parent);
    block->setProperty("class", QStringLiteral("ProfileFavoriteBlock"));
    block->setProperty("tone", tone);
    auto *lay = new QHBoxLayout(block);
    lay->setContentsMargins(18, 9, 14, 9);
    lay->setSpacing(12);
    auto *icon = new QLabel(block);
    icon->setObjectName(QStringLiteral("ProfileFavoriteIcon"));
    icon->setProperty("tone", tone);
    icon->setAlignment(Qt::AlignCenter);
    icon->setFixedSize(58, 58);
    icon->setPixmap(UiAssets::svgPixmap(iconName, QSize(34, 34), toneColor(tone), icon));
    auto *text = new QVBoxLayout;
    text->setSpacing(2);
    auto *label = new QLabel(caption, block);
    label->setObjectName(QStringLiteral("ProfileValueCaption"));
    *value = new QLabel(QStringLiteral("0"), block);
    (*value)->setObjectName(QStringLiteral("ProfileFavoriteValue"));
    (*value)->setProperty("tone", tone);
    text->addWidget(label);
    text->addWidget(*value);
    lay->addWidget(icon);
    lay->addLayout(text, 1);
    *openButton = new QPushButton(QStringLiteral("查看"), block);
    (*openButton)->setProperty("class", QStringLiteral("ProfileFavoriteOpen"));
    UiAssets::setButtonIcon(*openButton, QStringLiteral("chevron-right"), 14,
                            QColor(QStringLiteral("#14213D")));
    (*openButton)->setLayoutDirection(Qt::RightToLeft);
    (*openButton)->setFixedSize(72, 36);
    lay->addWidget(*openButton);
    return block;
}
} // namespace

ProfileWidget::ProfileWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(23, 1, 27, 23);
    root->setSpacing(11);

    auto *profileCard = new QFrame(this);
    profileCard->setObjectName(QStringLiteral("ProfileOverviewCard"));
    profileCard->setFixedHeight(294);
    auto *profileLay = new QVBoxLayout(profileCard);
    profileLay->setContentsMargins(0, 0, 0, 0);
    profileLay->setSpacing(0);
    auto *profileLeaves = new QLabel(profileCard);
    profileLeaves->setObjectName(QStringLiteral("ProfileLeafDecoration"));
    profileLeaves->setAttribute(Qt::WA_TransparentForMouseEvents);
    profileLeaves->setFixedSize(126, 118);
    profileLeaves->setPixmap(UiAssets::svgPixmap(
        QStringLiteral("profile-botanical"), QSize(126, 118), QColor(), profileLeaves));
    profileLeaves->move(-5, 0);
    profileLeaves->lower();

    auto *identity = new QWidget(profileCard);
    auto *identityLay = new QHBoxLayout(identity);
    identityLay->setContentsMargins(72, 12, 20, 12);
    identityLay->setSpacing(20);
    m_avatarLabel = new QLabel(QStringLiteral("膳"), identity);
    m_avatarLabel->setObjectName(QStringLiteral("ProfileAvatar"));
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    auto *nameBox = new QVBoxLayout;
    nameBox->setSpacing(4);
    m_nameLabel = new QLabel(QStringLiteral("张明"), identity);
    m_nameLabel->setObjectName(QStringLiteral("ProfileName"));
    m_goalChip = new QLabel(QStringLiteral("增肌计划"), identity);
    m_goalChip->setObjectName(QStringLiteral("ProfileGoalChip"));
    nameBox->addWidget(m_nameLabel);
    nameBox->addWidget(m_goalChip, 0, Qt::AlignLeft);
    auto *editBtn = new QPushButton(QStringLiteral("编辑资料"), identity);
    editBtn->setProperty("class", QStringLiteral("GhostButton"));
    UiAssets::setButtonIcon(editBtn, QStringLiteral("edit"), 18);
    identityLay->addWidget(m_avatarLabel);
    identityLay->addLayout(nameBox);
    identityLay->addStretch();
    identityLay->addWidget(editBtn);

    auto *firstStats = new QWidget(profileCard);
    firstStats->setObjectName(QStringLiteral("ProfileStatsRow"));
    auto *firstLay = new QHBoxLayout(firstStats);
    firstLay->setContentsMargins(20, 0, 20, 5);
    firstLay->setSpacing(12);
    QWidget *genderBlock = makeValueBlock(firstStats, QStringLiteral("性别"), QStringLiteral("gender"), QStringLiteral("green"), &m_genderValue);
    genderBlock->setFixedWidth(194);
    firstLay->addWidget(genderBlock);
    firstLay->addWidget(makeValueBlock(firstStats, QStringLiteral("身高"), QStringLiteral("height"), QStringLiteral("blue"), &m_heightValue), 1);
    firstLay->addWidget(makeValueBlock(firstStats, QStringLiteral("体重"), QStringLiteral("scale"), QStringLiteral("orange"), &m_weightValue), 1);
    firstLay->addWidget(makeValueBlock(firstStats, QStringLiteral("BMI"), QStringLiteral("bmi"), QStringLiteral("purple"), &m_bmiValue), 1);

    auto *secondStats = new QWidget(profileCard);
    secondStats->setObjectName(QStringLiteral("ProfileStatsRow"));
    auto *secondLay = new QHBoxLayout(secondStats);
    secondLay->setContentsMargins(20, 5, 20, 14);
    secondLay->setSpacing(12);
    QWidget *calorieBlock = makeValueBlock(secondStats, QStringLiteral("每日热量"), QStringLiteral("flame"), QStringLiteral("orange"), &m_calorieValue);
    QWidget *proteinBlock = makeValueBlock(secondStats, QStringLiteral("蛋白质"), QStringLiteral("protein"), QStringLiteral("green"), &m_proteinValue);
    calorieBlock->setFixedWidth(253);
    proteinBlock->setFixedWidth(242);
    secondLay->addWidget(calorieBlock);
    secondLay->addWidget(proteinBlock);
    secondLay->addWidget(makeValueBlock(secondStats, QStringLiteral("饮食偏好"), QStringLiteral("preference-bowl"), QStringLiteral("blue"), &m_preferenceValue), 1);

    profileLay->addWidget(identity);
    profileLay->addWidget(firstStats);
    profileLay->addWidget(secondStats);
    root->addWidget(profileCard);

    auto *healthCard = new QFrame(this);
    healthCard->setObjectName(QStringLiteral("ProfileHealthCard"));
    healthCard->setFixedHeight(143);
    auto *healthLay = new QVBoxLayout(healthCard);
    healthLay->setContentsMargins(20, 10, 20, 10);
    healthLay->setSpacing(7);
    auto *healthHead = new QHBoxLayout;
    auto *healthTitle = new QLabel(QStringLiteral("健康状况"), healthCard);
    healthTitle->setProperty("class", QStringLiteral("SectionTitle"));
    auto *healthEdit = new QPushButton(QStringLiteral("编辑健康状况"), healthCard);
    healthEdit->setProperty("class", QStringLiteral("GhostButton"));
    UiAssets::setButtonIcon(healthEdit, QStringLiteral("edit"), 18);
    healthHead->addWidget(healthTitle);
    healthHead->addStretch();
    healthHead->addWidget(healthEdit);
    auto *healthGrid = new QHBoxLayout;
    healthGrid->setSpacing(12);
    healthGrid->addWidget(makeHealthBlock(healthCard, QStringLiteral("过敏原"), QStringLiteral("shield"), QStringLiteral("green"), &m_allergyValue), 1);
    healthGrid->addWidget(makeHealthBlock(healthCard, QStringLiteral("食物不耐受"), QStringLiteral("stomach"), QStringLiteral("orange"), &m_intoleranceValue), 1);
    healthGrid->addWidget(makeHealthBlock(healthCard, QStringLiteral("医疗状况"), QStringLiteral("medical-heart"), QStringLiteral("blue"), &m_medicalValue), 1);
    healthGrid->addWidget(makeHealthBlock(healthCard, QStringLiteral("营养缺乏"), QStringLiteral("vitamin"), QStringLiteral("purple"), &m_deficiencyValue), 1);
    healthLay->addLayout(healthHead);
    healthLay->addLayout(healthGrid);
    root->addWidget(healthCard);

    auto *favoriteCard = new QFrame(this);
    favoriteCard->setObjectName(QStringLiteral("ProfileFavoriteCard"));
    favoriteCard->setFixedHeight(155);
    auto *favoriteLay = new QVBoxLayout(favoriteCard);
    favoriteLay->setContentsMargins(16, 8, 16, 14);
    favoriteLay->setSpacing(4);
    auto *favoriteTitle = new QLabel(QStringLiteral("我的收藏"), favoriteCard);
    favoriteTitle->setProperty("class", QStringLiteral("SectionTitle"));
    auto *favoriteHead = new QHBoxLayout;
    auto *favoriteRow = new QHBoxLayout;
    QPushButton *recipeOpen = nullptr;
    QPushButton *foodOpen = nullptr;
    QWidget *recipeFavorite = makeFavoriteBlock(favoriteCard, QStringLiteral("收藏食谱"),
                                                 QStringLiteral("fork-spoon"), QStringLiteral("orange"),
                                                 &m_recipeFavoriteCount, &recipeOpen);
    QWidget *foodFavorite = makeFavoriteBlock(favoriteCard, QStringLiteral("收藏食材"),
                                               QStringLiteral("basket"), QStringLiteral("green"),
                                               &m_foodFavoriteCount, &foodOpen);
    auto *viewBtn = new QPushButton(QStringLiteral("查看我的收藏"), favoriteCard);
    viewBtn->setProperty("class", QStringLiteral("GhostButton"));
    UiAssets::setButtonIcon(viewBtn, QStringLiteral("chevron-right"), 14,
                            QColor(QStringLiteral("#14213D")));
    viewBtn->setLayoutDirection(Qt::RightToLeft);
    viewBtn->setFixedSize(130, 32);
    favoriteHead->addWidget(favoriteTitle);
    favoriteHead->addStretch();
    favoriteHead->addWidget(viewBtn);
    favoriteRow->addWidget(recipeFavorite, 1);
    favoriteRow->addWidget(foodFavorite, 1);
    favoriteLay->addLayout(favoriteHead);
    favoriteLay->addLayout(favoriteRow);
    root->addWidget(favoriteCard);

    connect(editBtn, &QPushButton::clicked, this, &ProfileWidget::openSettingsRequested);
    connect(healthEdit, &QPushButton::clicked, this, &ProfileWidget::openSettingsRequested);
    connect(viewBtn, &QPushButton::clicked, this, &ProfileWidget::openFavoritesRequested);
    connect(recipeOpen, &QPushButton::clicked, this, &ProfileWidget::openFavoritesRequested);
    connect(foodOpen, &QPushButton::clicked, this, &ProfileWidget::openFavoritesRequested);
}

void ProfileWidget::setUser(const User &user)
{
    m_user = user;
    const double bmi = UserService().calculateBMI(user);
    const QString displayName = user.name.isEmpty() ? QStringLiteral("用户") : user.name;
    m_avatarLabel->setText(displayName.left(1));
    m_nameLabel->setText(displayName);
    m_goalChip->setText(goalToCn(user.goal));
    m_genderValue->setText(user.gender == QStringLiteral("female") ? QStringLiteral("女")
                                                                   : QStringLiteral("男"));
    m_heightValue->setText(QStringLiteral("%1 cm").arg(user.height, 0, 'f', 0));
    m_weightValue->setText(QStringLiteral("%1 kg").arg(user.weight, 0, 'f', 0));
    m_bmiValue->setText(QString::number(bmi, 'f', 1));
    m_calorieValue->setText(QStringLiteral("%1 kcal").arg(user.calorieTarget));
    m_proteinValue->setText(QStringLiteral("%1 g").arg(qRound(user.weight * 1.8)));
    const QString preferenceText = !user.dietaryChoices.isEmpty()
        ? user.dietaryChoices.join(QStringLiteral("、")) : user.preferences.trimmed();
    m_preferenceValue->setText(preferenceText.isEmpty() ? QStringLiteral("未设置")
                                                        : preferenceText);
    refreshHealthSummary();
}

void ProfileWidget::refreshHealthSummary()
{
    const QStringList allergies = m_user.allergies.isEmpty()
                                      ? User::splitLegacyText(m_user.allergens)
                                      : m_user.allergies;
    auto textOr = [](const QStringList &values, const QString &empty) {
        return values.isEmpty() ? empty : values.join(QStringLiteral("、"));
    };
    m_allergyValue->setText(textOr(allergies, QStringLiteral("暂无记录")));
    m_intoleranceValue->setText(textOr(m_user.foodIntolerances, QStringLiteral("暂无记录")));
    m_medicalValue->setText(textOr(m_user.medicalConditions, QStringLiteral("暂无记录")));
    m_deficiencyValue->setText(textOr(m_user.nutritionalDeficiencies, QStringLiteral("暂无记录")));
}

void ProfileWidget::reloadFavorites()
{
    const int count = m_user.id > 0 ? RecipeDAO().findFavorites(m_user.id).size() : 0;
    const int foodCount = m_user.id > 0 ? FoodDAO().findFavorites(m_user.id).size() : 0;
    m_recipeFavoriteCount->setText(QString::number(count));
    m_foodFavoriteCount->setText(QString::number(foodCount));
}
