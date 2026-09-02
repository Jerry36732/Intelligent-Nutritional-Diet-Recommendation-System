#include "FoodDetailDialog.h"

#include "UiAssets.h"

#include "../dao/FoodDAO.h"
#include "../services/RecipeImageProvider.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

namespace {
QFrame *makeNutritionMetric(QWidget *parent, const QString &caption, const QString &value,
                            const QString &unit, const QString &tone)
{
    auto *card = new QFrame(parent);
    card->setProperty("class", QStringLiteral("FoodNutritionMetric"));
    card->setProperty("tone", tone);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(7, 7, 7, 7);
    layout->setSpacing(1);
    auto *label = new QLabel(caption, card);
    label->setObjectName(QStringLiteral("FoodMetricCaption"));
    auto *number = new QLabel(value, card);
    number->setObjectName(QStringLiteral("FoodMetricValue"));
    number->setProperty("tone", tone);
    auto *unitLabel = new QLabel(unit, card);
    unitLabel->setObjectName(QStringLiteral("FoodMetricUnit"));
    for (QLabel *item : {label, number, unitLabel})
        item->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
    layout->addWidget(number);
    layout->addWidget(unitLabel);
    return card;
}
} // namespace

FoodDetailDialog::FoodDetailDialog(const Food &food, const QString &category, int userId,
                                   QWidget *parent, int reviewFavoriteState)
    : QDialog(parent)
    , m_food(food)
    , m_category(category)
    , m_userId(userId)
    , m_reviewFavoriteState(reviewFavoriteState)
{
    setObjectName(QStringLiteral("FoodDetailDialog"));
    setWindowFlag(Qt::FramelessWindowHint, true);
    setModal(true);
    setFixedSize(500, 498);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(26, 16, 26, 14);
    root->setSpacing(0);
    auto *header = new QHBoxLayout;
    auto *caption = new QLabel(QStringLiteral("食材详情"), this);
    caption->setObjectName(QStringLiteral("DialogCaption"));
    caption->setFont(UiAssets::titleFont(21));
    auto *closeTop = new QPushButton(this);
    closeTop->setObjectName(QStringLiteral("DialogCloseButton"));
    closeTop->setFixedSize(30, 30);
    UiAssets::setButtonIcon(closeTop, QStringLiteral("close"), 18);
    header->addWidget(caption);
    header->addStretch();
    header->addWidget(closeTop);

    auto *hero = new QHBoxLayout;
    hero->setSpacing(18);
    auto *image = new QLabel(this);
    image->setObjectName(QStringLiteral("FoodDetailImage"));
    image->setFixedSize(137, 127);
    image->setPixmap(RecipeImageProvider::pixmap(m_food.name, image->size()));
    image->setScaledContents(true);
    auto *heroCopy = new QVBoxLayout;
    heroCopy->setSpacing(5);
    auto *name = new QLabel(m_food.name, this);
    name->setObjectName(QStringLiteral("FoodDetailName"));
    auto *categoryRow = new QHBoxLayout;
    categoryRow->setSpacing(7);
    auto *categoryDot = new QFrame(this);
    categoryDot->setObjectName(QStringLiteral("FoodDetailCategoryDot"));
    categoryDot->setFixedSize(8, 8);
    auto *categoryLabel = new QLabel(m_category, this);
    categoryLabel->setObjectName(QStringLiteral("FoodDetailCategory"));
    categoryRow->addWidget(categoryDot, 0, Qt::AlignVCenter);
    categoryRow->addWidget(categoryLabel);
    categoryRow->addStretch();
    auto *summary = new QLabel(
        QStringLiteral("富含多种基础营养素，口感自然，适合作为健康日常饮食的一部分。"),
        this);
    summary->setObjectName(QStringLiteral("FoodDetailSummary"));
    summary->setWordWrap(true);
    heroCopy->addWidget(name);
    heroCopy->addLayout(categoryRow);
    heroCopy->addWidget(summary);
    heroCopy->addStretch();
    hero->addWidget(image);
    hero->addLayout(heroCopy, 1);

    auto *nutritionTitle = new QLabel(QStringLiteral("每100g营养成分"), this);
    nutritionTitle->setObjectName(QStringLiteral("FoodDetailSectionTitle"));
    auto *nutrition = new QHBoxLayout;
    nutrition->setSpacing(12);
    nutrition->addWidget(makeNutritionMetric(this, QStringLiteral("热量"),
                                QString::number(m_food.calories, 'f', 0),
                                QStringLiteral("kcal"), QStringLiteral("green")), 1);
    nutrition->addWidget(makeNutritionMetric(this, QStringLiteral("蛋白质"),
                                QString::number(m_food.protein, 'f', 1),
                                QStringLiteral("g"), QStringLiteral("blue")), 1);
    nutrition->addWidget(makeNutritionMetric(this, QStringLiteral("脂肪"),
                                QString::number(m_food.fat, 'f', 1),
                                QStringLiteral("g"), QStringLiteral("orange")), 1);
    nutrition->addWidget(makeNutritionMetric(this, QStringLiteral("碳水"),
                                QString::number(m_food.carbs, 'f', 1),
                                QStringLiteral("g"), QStringLiteral("cyan")), 1);

    auto *rule = new QFrame(this);
    rule->setObjectName(QStringLiteral("FoodDetailRule"));
    rule->setFixedHeight(1);
    auto *details = new QHBoxLayout;
    details->setSpacing(24);
    auto makeDetail = [this](const QString &title, const QStringList &items) {
        auto *host = new QWidget(this);
        auto *layout = new QVBoxLayout(host);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);
        auto *caption = new QLabel(title, host);
        caption->setObjectName(QStringLiteral("FoodDetailColumnTitle"));
        layout->addWidget(caption);
        for (const QString &item : items) {
            auto *lineRow = new QHBoxLayout;
            lineRow->setContentsMargins(0, 0, 0, 0);
            lineRow->setSpacing(7);
            auto *dot = new QFrame(host);
            dot->setObjectName(QStringLiteral("FoodDetailBulletDot"));
            dot->setFixedSize(4, 4);
            auto *line = new QLabel(item, host);
            line->setObjectName(QStringLiteral("FoodDetailBullet"));
            line->setWordWrap(true);
            lineRow->addWidget(dot, 0, Qt::AlignTop);
            lineRow->addWidget(line, 1);
            layout->addLayout(lineRow);
        }
        layout->addStretch();
        return host;
    };
    details->addWidget(makeDetail(
        QStringLiteral("营养特点"),
        {QStringLiteral("营养组合丰富，有助于维持日常活力"),
         QStringLiteral("天然食材，适合搭配多样膳食"),
         QStringLiteral("合理控制份量，有助于均衡饮食")}));
    details->addWidget(makeDetail(
        QStringLiteral("食用建议"),
        {QStringLiteral("建议结合个人目标调整每日份量"),
         QStringLiteral("搭配蔬菜、奶类或优质蛋白"),
         QStringLiteral("可蒸煮、焖炖或简单快炒")}));

    auto *footerRule = new QFrame(this);
    footerRule->setObjectName(QStringLiteral("FoodDetailRule"));
    footerRule->setFixedHeight(1);
    auto *footer = new QHBoxLayout;
    m_favoriteButton = new QPushButton(this);
    m_favoriteButton->setObjectName(QStringLiteral("FoodDetailFavoriteButton"));
    m_favoriteButton->setCheckable(true);
    m_favoriteButton->setFixedHeight(34);
    auto *stateHint = new QLabel(this);
    stateHint->setObjectName(QStringLiteral("FoodDetailStateHint"));
    auto *close = new QPushButton(QStringLiteral("关闭"), this);
    close->setObjectName(QStringLiteral("FoodDetailCloseButton"));
    close->setFixedSize(88, 34);
    footer->addWidget(m_favoriteButton);
    footer->addWidget(stateHint, 1);
    footer->addWidget(close);

    root->addLayout(header);
    root->addSpacing(13);
    root->addLayout(hero);
    root->addSpacing(24);
    root->addWidget(nutritionTitle);
    root->addSpacing(6);
    root->addLayout(nutrition);
    root->addSpacing(10);
    root->addWidget(rule);
    root->addSpacing(10);
    root->addLayout(details, 1);
    root->addWidget(footerRule);
    root->addSpacing(8);
    root->addLayout(footer);

    m_favorite = m_reviewFavoriteState >= 0
        ? (m_reviewFavoriteState != 0)
        : (m_userId > 0 && FoodDAO().isFavorite(m_userId, m_food.id));
    refreshFavoriteButton();
    stateHint->setText(m_favorite ? QStringLiteral("已加入我的收藏，再次点击可取消收藏")
                                  : QStringLiteral("点击收藏后星标填充，并加入我的收藏"));
    connect(closeTop, &QPushButton::clicked, this, &QDialog::accept);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_favoriteButton, &QPushButton::clicked, this, [this, stateHint](bool desired) {
        if (m_userId <= 0) {
            m_favoriteButton->setChecked(false);
            QMessageBox::information(this, QStringLiteral("请先登录"),
                                     QStringLiteral("登录后可以收藏食材。"));
            return;
        }
        if (!FoodDAO().setFavorite(m_userId, m_food.id, desired)) {
            m_favoriteButton->setChecked(!desired);
            QMessageBox::warning(this, QStringLiteral("收藏失败"),
                                 QStringLiteral("收藏状态未保存，请稍后重试。"));
            return;
        }
        m_favorite = desired;
        refreshFavoriteButton();
        stateHint->setText(m_favorite ? QStringLiteral("已加入我的收藏，再次点击可取消收藏")
                                      : QStringLiteral("点击收藏后星标填充，并加入我的收藏"));
        emit favoriteChanged(m_food.id, m_favorite);
    });
}

void FoodDetailDialog::refreshFavoriteButton()
{
    m_favoriteButton->setChecked(m_favorite);
    m_favoriteButton->setText(m_favorite ? QStringLiteral("取消收藏")
                                         : QStringLiteral("收藏食材"));
    UiAssets::setButtonIcon(m_favoriteButton,
                            m_favorite ? QStringLiteral("star-filled")
                                       : QStringLiteral("star-outline"),
                            20, QColor(QStringLiteral("#059669")));
}

void FoodDetailDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (QWidget *owner = parentWidget()) {
        const QRect rect = owner->frameGeometry();
        move(rect.center().x() - width() / 2,
             rect.center().y() - height() / 2 + 19);
    }
}
