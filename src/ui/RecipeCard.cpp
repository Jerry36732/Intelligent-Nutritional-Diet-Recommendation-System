#include "RecipeCard.h"
#include "UiAssets.h"

#include "../services/RecipeImageProvider.h"

#include <QHBoxLayout>
#include <QBoxLayout>
#include <QEvent>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>
#include <QVBoxLayout>
#include <QtGlobal>

namespace {
void clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget())
            delete widget;
        if (QLayout *child = item->layout())
            clearLayout(child);
        delete item;
    }
}

QString ratioForMeal(const QString &meal)
{
    if (meal == QStringLiteral("早餐"))
        return QStringLiteral("建议 25%");
    if (meal == QStringLiteral("午餐"))
        return QStringLiteral("建议 40%");
    if (meal == QStringLiteral("晚餐"))
        return QStringLiteral("建议 35%");
    return QStringLiteral("营养均衡");
}
} // namespace

RecipeCard::RecipeCard(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("RecipeCard"));
    setProperty("class", QVariant(QStringLiteral("RecipeCard")));
    setFrameShape(QFrame::StyledPanel);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::PointingHandCursor);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 9, 10, 8);
    root->setSpacing(6);

    auto *header = new QHBoxLayout;
    header->setSpacing(8);
    m_mealIcon = new QLabel(this);
    m_mealIcon->setObjectName(QStringLiteral("MealIcon"));
    m_mealIcon->setFixedSize(40, 40);
    m_mealIcon->setAlignment(Qt::AlignCenter);
    m_mealTag = new QLabel(this);
    m_mealTag->setObjectName(QStringLiteral("MealTag"));
    m_mealTag->setProperty("class", QVariant(QStringLiteral("MealTag")));
    m_ratioLabel = new QLabel(this);
    m_ratioLabel->setObjectName(QStringLiteral("MealRatio"));
    m_totalKcalLabel = new QLabel(this);
    m_totalKcalLabel->setObjectName(QStringLiteral("MealTotalKcal"));
    m_totalKcalLabel->setAlignment(Qt::AlignCenter);
    auto *mealCopy = new QVBoxLayout;
    mealCopy->setSpacing(1);
    mealCopy->addWidget(m_mealTag);
    mealCopy->addWidget(m_ratioLabel);
    header->addWidget(m_mealIcon);
    header->addLayout(mealCopy);
    header->addStretch();

    auto *dishHost = new QWidget(this);
    dishHost->setObjectName(QStringLiteral("MealDishHost"));
    m_dishLayout = new QGridLayout(dishHost);
    m_dishLayout->setContentsMargins(0, 0, 0, 0);
    m_dishLayout->setHorizontalSpacing(6);
    m_dishLayout->setVerticalSpacing(5);

    auto *footer = new QHBoxLayout;
    footer->setSpacing(7);
    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setObjectName(QStringLiteral("MealNutrientSummary"));
    m_summaryLabel->setWordWrap(true);
    m_favBtn = new QPushButton(this);
    m_favBtn->setCheckable(true);
    m_favBtn->setCursor(Qt::PointingHandCursor);
    m_favBtn->setObjectName(QStringLiteral("HeartButton"));
    m_favBtn->setProperty("class", QVariant(QStringLiteral("HeartButton")));
    m_favBtn->setToolTip(QStringLiteral("收藏本餐主菜"));
    UiAssets::setButtonIcon(m_favBtn, QStringLiteral("star-outline"), 22);
    m_detailBtn = new QPushButton(QStringLiteral("查看详情"), this);
    m_detailBtn->setObjectName(QStringLiteral("MealDetailButton"));
    m_detailBtn->setCursor(Qt::PointingHandCursor);
    m_detailBtn->setProperty("class", QVariant(QStringLiteral("GhostButton")));
    footer->addStretch(1);
    footer->addWidget(m_totalKcalLabel, 0, Qt::AlignCenter);
    footer->addStretch(1);
    // 餐次卡片本身就是查看入口，避免在紧凑卡片中重复显示爱心和详情按钮。
    m_favBtn->hide();
    m_detailBtn->hide();
    m_summaryLabel->hide();

    root->addLayout(header);
    root->addWidget(dishHost, 1);
    root->addLayout(footer);

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
        UiAssets::setButtonIcon(m_favBtn,
                                m_favBtn->isChecked() ? QStringLiteral("star-filled")
                                                      : QStringLiteral("star-outline"),
                                22, QColor(QStringLiteral("#08A96E")));
        emit favoriteToggled(m_meal.primary().id);
    });

    clear();
}

void RecipeCard::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())
        && m_meal.isValid()) {
        if (m_meal.dishes.size() > 1)
            emit mealDetailRequested(m_meal);
        else
            emit detailClicked(m_meal.primary());
        event->accept();
        return;
    }
    QFrame::mouseReleaseEvent(event);
}

bool RecipeCard::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease && m_rowRecipes.contains(watched)) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            const Recipe recipe = m_rowRecipes.value(watched);
            if (recipe.isValid())
                emit detailClicked(recipe);
            return true;
        }
    }
    return QFrame::eventFilter(watched, event);
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
    UiAssets::setButtonIcon(m_favBtn,
                            favorited ? QStringLiteral("star-filled")
                                      : QStringLiteral("star-outline"),
                            22, favorited ? QColor(QStringLiteral("#F2A23A"))
                                          : QColor(QStringLiteral("#8A94A6")));
}

bool RecipeCard::isFavorited() const
{
    return m_favBtn->isChecked();
}

void RecipeCard::clear()
{
    m_meal = MealSlot{};
    applyMealUi();
    setFavorited(false);
}

void RecipeCard::rebuildDishes()
{
    m_rowRecipes.clear();
    clearLayout(m_dishLayout);
    if (!m_meal.isValid()) {
        auto *empty = new QLabel(QStringLiteral("尚未生成本餐方案"), this);
        empty->setObjectName(QStringLiteral("MealEmpty"));
        empty->setAlignment(Qt::AlignCenter);
        m_dishLayout->addWidget(empty, 0, 0, 1, 2);
        return;
    }

    const int count = m_meal.dishes.size();
    for (int dishIndex = 0; dishIndex < count; ++dishIndex) {
        const Recipe dish = m_meal.dishes.at(dishIndex);
        const bool compact = count >= 3;
        auto *row = new QFrame(this);
        row->setObjectName(QStringLiteral("MealDishRow"));
        row->setProperty("compact", compact);
        row->setCursor(Qt::PointingHandCursor);
        row->setToolTip(QStringLiteral("查看“%1”详情").arg(dish.name));
        row->setAccessibleName(QStringLiteral("查看食谱：%1").arg(dish.name));
        row->installEventFilter(this);
        m_rowRecipes.insert(row, dish);
        auto *rowLayout = new QBoxLayout(compact ? QBoxLayout::TopToBottom
                                                  : QBoxLayout::LeftToRight, row);
        rowLayout->setContentsMargins(compact ? 2 : 5, compact ? 2 : 4,
                                      compact ? 2 : 5, compact ? 2 : 4);
        rowLayout->setSpacing(compact ? 2 : 7);

        auto *image = new QLabel(row);
        image->setObjectName(QStringLiteral("RecipeThumb"));
        if (compact) {
            const bool wideFirst = count == 3 && dishIndex == 0;
            image->setFixedSize(wideFirst ? QSize(208, 52) : QSize(101, 48));
        } else {
            image->setFixedSize(84, 54);
        }
        image->setPixmap(RecipeImageProvider::pixmap(dish.name, image->size()));
        image->setAttribute(Qt::WA_TransparentForMouseEvents);

        auto *text = new QVBoxLayout;
        text->setSpacing(1);
        auto *name = new QLabel(dish.name, row);
        name->setObjectName(QStringLiteral("DishName"));
        name->setWordWrap(true);
        name->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto *meta = new QLabel(
            QStringLiteral("%1 kcal").arg(static_cast<int>(qRound(dish.totalCalories))), row);
        meta->setObjectName(QStringLiteral("DishMeta"));
        meta->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto *macro = new QLabel(
            QStringLiteral("P %1g   C %2g   F %3g")
                .arg(dish.totalProtein, 0, 'f', 1)
                .arg(dish.totalCarbs, 0, 'f', 1)
                .arg(dish.totalFat, 0, 'f', 1),
            row);
        macro->setObjectName(QStringLiteral("DishMacro"));
        macro->setAttribute(Qt::WA_TransparentForMouseEvents);
        text->addWidget(name);
        if (!compact) {
            text->addWidget(meta);
            text->addWidget(macro);
        } else {
            meta->hide();
            macro->hide();
        }
        rowLayout->addWidget(image, 0, Qt::AlignVCenter);
        rowLayout->addLayout(text, 1);
        if (count == 3) {
            if (dishIndex == 0)
                m_dishLayout->addWidget(row, 0, 0, 1, 2);
            else
                m_dishLayout->addWidget(row, 1, dishIndex - 1);
        } else if (count >= 4) {
            m_dishLayout->addWidget(row, dishIndex / 2, dishIndex % 2);
        } else {
            m_dishLayout->addWidget(row, dishIndex, 0, 1, 2);
        }
    }
    m_dishLayout->setRowStretch(count >= 3 ? 2 : count, 1);
}

void RecipeCard::rebuildSummary()
{
    if (!m_meal.isValid()) {
        m_summaryLabel->setText(QStringLiteral("P —   C —   F —"));
        return;
    }
    m_summaryLabel->setText(
        QStringLiteral("小计 %1 kcal\nP %2g   C %3g   F %4g")
            .arg(static_cast<int>(qRound(m_meal.totalCalories())))
            .arg(m_meal.totalProtein(), 0, 'f', 1)
            .arg(m_meal.totalCarbs(), 0, 'f', 1)
            .arg(m_meal.totalFat(), 0, 'f', 1));
}

void RecipeCard::applyMealUi()
{
    const QString meal = m_meal.mealLabel.isEmpty() ? QStringLiteral("餐次") : m_meal.mealLabel;
    m_mealTag->setText(meal);
    m_mealTag->setProperty("meal", meal);
    m_mealTag->style()->unpolish(m_mealTag);
    m_mealTag->style()->polish(m_mealTag);
    m_ratioLabel->setText(ratioForMeal(meal));
    m_totalKcalLabel->setText(m_meal.isValid()
                                  ? QStringLiteral("%1 kcal").arg(static_cast<int>(qRound(m_meal.totalCalories())))
                                  : QStringLiteral("— kcal"));
    QString iconName = QStringLiteral("steaming-bowl");
    QColor iconColor(Qt::white);
    if (meal == QStringLiteral("早餐")) {
        iconName = QStringLiteral("sun");
        m_totalKcalLabel->setProperty("tone", QStringLiteral("orange"));
        m_mealIcon->setProperty("tone", QStringLiteral("orange"));
    } else if (meal == QStringLiteral("晚餐")) {
        iconName = QStringLiteral("moon");
        m_totalKcalLabel->setProperty("tone", QStringLiteral("blue"));
        m_mealIcon->setProperty("tone", QStringLiteral("blue"));
    } else {
        m_totalKcalLabel->setProperty("tone", QStringLiteral("green"));
        m_mealIcon->setProperty("tone", QStringLiteral("green"));
    }
    m_totalKcalLabel->style()->unpolish(m_totalKcalLabel);
    m_totalKcalLabel->style()->polish(m_totalKcalLabel);
    m_mealIcon->style()->unpolish(m_mealIcon);
    m_mealIcon->style()->polish(m_mealIcon);
    m_mealIcon->setPixmap(UiAssets::svgPixmap(iconName, QSize(24, 24), iconColor, m_mealIcon));
    setProperty("meal", meal);
    style()->unpolish(this);
    style()->polish(this);
    rebuildDishes();
    rebuildSummary();
    m_detailBtn->setEnabled(m_meal.isValid());
    m_favBtn->setEnabled(m_meal.isValid());
}
