#include "RecipeCard.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QtGlobal>
#include <QVBoxLayout>

RecipeCard::RecipeCard(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RecipeCard"));
    setProperty("class", QVariant(QStringLiteral("RecipeCard")));
    setFrameShape(QFrame::StyledPanel);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(10);

    auto *top = new QHBoxLayout;
    m_mealTag = new QLabel(this);
    m_mealTag->setProperty("class", QVariant(QStringLiteral("MealTag")));
    m_mealTag->setObjectName(QStringLiteral("MealTag"));

    m_favBtn = new QPushButton(QStringLiteral("♡"), this);
    m_favBtn->setCheckable(true);
    m_favBtn->setCursor(Qt::PointingHandCursor);
    m_favBtn->setProperty("class", QVariant(QStringLiteral("HeartButton")));
    m_favBtn->setObjectName(QStringLiteral("HeartButton"));

    top->addWidget(m_mealTag);
    top->addStretch();
    top->addWidget(m_favBtn);

    m_nameLabel = new QLabel(this);
    m_nameLabel->setProperty("class", QVariant(QStringLiteral("CardTitle")));
    m_nameLabel->setWordWrap(true);

    m_metaLabel = new QLabel(this);
    m_metaLabel->setProperty("class", QVariant(QStringLiteral("CardMeta")));
    m_metaLabel->setWordWrap(true);

    auto *nutrients = new QHBoxLayout;
    nutrients->setSpacing(8);
    m_proteinChip = new QLabel(this);
    m_carbsChip = new QLabel(this);
    m_fatChip = new QLabel(this);
    for (QLabel *chip : {m_proteinChip, m_carbsChip, m_fatChip}) {
        chip->setProperty("class", QVariant(QStringLiteral("NutrientChip")));
        nutrients->addWidget(chip);
    }
    nutrients->addStretch();

    m_detailBtn = new QPushButton(QStringLiteral("查看详情"), this);
    m_detailBtn->setCursor(Qt::PointingHandCursor);
    m_detailBtn->setProperty("class", QVariant(QStringLiteral("GhostButton")));

    root->addLayout(top);
    root->addWidget(m_nameLabel);
    root->addWidget(m_metaLabel);
    root->addLayout(nutrients);
    root->addStretch();
    root->addWidget(m_detailBtn, 0, Qt::AlignLeft);

    connect(m_detailBtn, &QPushButton::clicked, this, [this]() {
        if (!m_meal.isValid())
            return;
        if (m_meal.dishes.size() > 1)
            emit mealDetailRequested(m_meal);
        else
            emit detailClicked(m_meal.primary());
    });
    connect(m_favBtn, &QPushButton::clicked, this, [this]() {
        if (!m_meal.isValid())
            return;
        m_favBtn->setText(m_favBtn->isChecked() ? QStringLiteral("♥") : QStringLiteral("♡"));
        emit favoriteToggled(m_meal.primary().id);
    });

    clear();
}

void RecipeCard::setRecipe(const Recipe &recipe)
{
    MealSlot slot;
    slot.mealLabel = recipe.category.isEmpty() ? QStringLiteral("餐次") : recipe.category;
    if (recipe.isValid())
        slot.dishes.append(recipe);
    setMeal(slot);
}

void RecipeCard::setMeal(const MealSlot &meal)
{
    m_meal = meal;
    applyMealUi();
}

Recipe RecipeCard::recipe() const
{
    return m_meal.primary();
}

MealSlot RecipeCard::meal() const
{
    return m_meal;
}

void RecipeCard::setFavorited(bool favorited)
{
    m_favBtn->setChecked(favorited);
    m_favBtn->setText(favorited ? QStringLiteral("♥") : QStringLiteral("♡"));
}

bool RecipeCard::isFavorited() const
{
    return m_favBtn->isChecked();
}

void RecipeCard::clear()
{
    m_meal = MealSlot{};
    m_mealTag->setText(QStringLiteral("餐次"));
    m_mealTag->setProperty("meal", QString());
    m_nameLabel->setText(QStringLiteral("等待生成方案"));
    m_metaLabel->setText(QStringLiteral("— kcal · — 分钟"));
    m_proteinChip->setText(QStringLiteral("P —"));
    m_carbsChip->setText(QStringLiteral("C —"));
    m_fatChip->setText(QStringLiteral("F —"));
    setFavorited(false);
}

void RecipeCard::applyMealUi()
{
    const QString tag = m_meal.mealLabel.isEmpty() ? QStringLiteral("餐次") : m_meal.mealLabel;
    m_mealTag->setText(tag);
    m_mealTag->setProperty("meal", tag);
    m_mealTag->style()->unpolish(m_mealTag);
    m_mealTag->style()->polish(m_mealTag);

    if (!m_meal.isValid()) {
        m_nameLabel->setText(QStringLiteral("暂无推荐"));
        m_metaLabel->setText(QStringLiteral("— kcal · — 分钟"));
        rebuildNutrients();
        setEnabled(false);
        return;
    }

    m_nameLabel->setText(m_meal.title());
    const QString dishCount = m_meal.dishes.size() > 1
                                  ? QStringLiteral("%1 道菜 · ").arg(m_meal.dishes.size())
                                  : QString();
    m_metaLabel->setText(QStringLiteral("%1%2 kcal · %3 分钟")
                             .arg(dishCount)
                             .arg(static_cast<int>(qRound(m_meal.totalCalories())))
                             .arg(m_meal.cookMinutes()));
    rebuildNutrients();
    setEnabled(true);
}

void RecipeCard::rebuildNutrients()
{
    m_proteinChip->setText(QStringLiteral("P %1g").arg(m_meal.totalProtein(), 0, 'f', 1));
    m_carbsChip->setText(QStringLiteral("C %1g").arg(m_meal.totalCarbs(), 0, 'f', 1));
    m_fatChip->setText(QStringLiteral("F %1g").arg(m_meal.totalFat(), 0, 'f', 1));
}
