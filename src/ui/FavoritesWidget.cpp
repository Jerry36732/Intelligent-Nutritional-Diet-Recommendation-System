#include "FavoritesWidget.h"
#include "RecipeEditorDialog.h"
#include "UiAssets.h"

#include "../dao/FoodDAO.h"
#include "../dao/PersonalRecipeDAO.h"
#include "../dao/RecipeDAO.h"
#include "../services/RecipeImageProvider.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QSet>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
void clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget())
            delete widget;
        delete item;
    }
}
}

FavoritesWidget::FavoritesWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 8, 30, 22);
    root->setSpacing(12);

    auto *subtitle = new QLabel(QStringLiteral("收藏的食谱和食材会保存在本地档案中。"), this);
    subtitle->setObjectName(QStringLiteral("PageSubtitle"));
    root->addWidget(subtitle);

    auto makePanel = [this](const QString &title, QLabel **count, QVBoxLayout **rows) {
        auto *panel = new QFrame(this);
        panel->setObjectName(QStringLiteral("FavoritePanel"));
        const QString tone = title.contains(QStringLiteral("食谱"))
                                 ? QStringLiteral("orange") : QStringLiteral("blue");
        panel->setProperty("tone", tone);
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(8);
        auto *headerHost = new QFrame(panel);
        headerHost->setProperty("class", QStringLiteral("FavoritePanelHeader"));
        headerHost->setProperty("tone", tone);
        auto *header = new QHBoxLayout(headerHost);
        header->setContentsMargins(10, 7, 10, 7);
        auto *headerIcon = UiAssets::createIconLabel(
            headerHost,
            tone == QStringLiteral("orange") ? QStringLiteral("fork-spoon")
                                               : QStringLiteral("basket"),
            22, tone == QStringLiteral("orange") ? QColor(QStringLiteral("#C87B38"))
                                                   : QColor(QStringLiteral("#5574C9")));
        auto *label = new QLabel(title, headerHost);
        label->setObjectName(QStringLiteral("FavoritePanelTitle"));
        *count = new QLabel(QStringLiteral("0"), panel);
        (*count)->setObjectName(QStringLiteral("FavoritePanelCount"));
        header->addWidget(headerIcon);
        header->addWidget(label);
        header->addStretch();
        header->addWidget(*count);
        auto *scroll = new QScrollArea(panel);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto *host = new QWidget(scroll);
        *rows = new QVBoxLayout(host);
        (*rows)->setContentsMargins(0, 0, 0, 0);
        (*rows)->setSpacing(7);
        scroll->setWidget(host);
        layout->addWidget(headerHost);
        layout->addWidget(scroll, 1);
        return panel;
    };

    auto *body = new QHBoxLayout;
    body->setSpacing(14);
    body->addWidget(makePanel(QStringLiteral("收藏食谱"), &m_recipeCount, &m_recipeRows), 53);
    body->addWidget(makePanel(QStringLiteral("收藏食材"), &m_foodCount, &m_foodRows), 47);
    subtitle->hide();
    auto *searchRow = new QHBoxLayout;
    searchRow->setContentsMargins(0, 14, 0, 0);
    auto *webImport = new QPushButton(QStringLiteral("网页导入"), this);
    webImport->setProperty("class", QStringLiteral("GhostButton"));
    UiAssets::setButtonIcon(webImport, QStringLiteral("refresh"), 17);
    auto *manualCreate = new QPushButton(QStringLiteral("手动创建"), this);
    manualCreate->setProperty("class", QStringLiteral("PrimaryButton"));
    UiAssets::setButtonIcon(manualCreate, QStringLiteral("plus"), 17, QColor(Qt::white));
    searchRow->addWidget(webImport);
    searchRow->addWidget(manualCreate);
    searchRow->addStretch();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName(QStringLiteral("FavoriteSearchInput"));
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索收藏的食谱或食材"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedWidth(286);
    m_searchEdit->addAction(UiAssets::svgIcon(QStringLiteral("search")), QLineEdit::LeadingPosition);
    searchRow->addWidget(m_searchEdit);
    root->addLayout(searchRow);
    root->addLayout(body, 1);
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(180);
    connect(m_searchTimer, &QTimer::timeout, this, &FavoritesWidget::reload);
    connect(m_searchEdit, &QLineEdit::textChanged, this,
            [this]() { m_searchTimer->start(); });
    connect(webImport, &QPushButton::clicked, this, &FavoritesWidget::openWebImporter);
    connect(manualCreate, &QPushButton::clicked, this, &FavoritesWidget::openManualCreator);
}

FavoritesWidget::~FavoritesWidget() = default;

void FavoritesWidget::openWebImporter()
{
    if (m_user.id <= 0)
        return;
    RecipeEditorDialog dialog(m_user.id, RecipeEditorDialog::Mode::WebImport, this);
    connect(&dialog, &RecipeEditorDialog::recipeCreated, this, [this](int recipeId) {
        reload();
        emit personalRecipeCreated(recipeId);
    });
    dialog.exec();
}

void FavoritesWidget::openManualCreator()
{
    if (m_user.id <= 0)
        return;
    RecipeEditorDialog dialog(m_user.id, RecipeEditorDialog::Mode::Manual, this);
    connect(&dialog, &RecipeEditorDialog::recipeCreated, this, [this](int recipeId) {
        reload();
        emit personalRecipeCreated(recipeId);
    });
    dialog.exec();
}

void FavoritesWidget::setUser(const User &user)
{
    m_user = user;
}

void FavoritesWidget::clearRows()
{
    clearLayout(m_recipeRows);
    clearLayout(m_foodRows);
}

void FavoritesWidget::reload()
{
    clearRows();
    RecipeDAO dao;
    QList<Recipe> favorites = m_user.id > 0 ? dao.findFavorites(m_user.id) : QList<Recipe>{};
    const QList<Recipe> personal = m_user.id > 0
        ? PersonalRecipeDAO().findByUser(m_user.id) : QList<Recipe>{};
    QSet<int> favoritedIds;
    QSet<int> personalIds;
    for (const Recipe &recipe : favorites)
        favoritedIds.insert(recipe.id);
    for (const Recipe &recipe : personal) {
        personalIds.insert(recipe.id);
        if (!favoritedIds.contains(recipe.id))
            favorites.append(recipe);
    }
    const QList<Food> foodFavorites = m_user.id > 0 ? FoodDAO().findFavorites(m_user.id) : QList<Food>{};
    m_recipeCount->setText(QString::number(favorites.size()));
    m_foodCount->setText(QString::number(foodFavorites.size()));

    const QString filter = m_searchEdit ? m_searchEdit->text().trimmed() : QString();
    QList<Recipe> shownRecipes;
    for (const Recipe &recipe : favorites) {
        if (filter.isEmpty() || recipe.name.contains(filter, Qt::CaseInsensitive)
            || recipe.category.contains(filter, Qt::CaseInsensitive)) {
            shownRecipes.append(recipe);
        }
    }
    QList<Food> shownFoods;
    for (const Food &food : foodFavorites) {
        if (filter.isEmpty() || food.name.contains(filter, Qt::CaseInsensitive)
            || food.category.contains(filter, Qt::CaseInsensitive)) {
            shownFoods.append(food);
        }
    }

    if (shownRecipes.isEmpty()) {
        auto *empty = new QLabel(filter.isEmpty()
                                     ? QStringLiteral("暂无收藏食谱\n可在食谱详情中点击星标收藏。")
                                     : QStringLiteral("未找到匹配的收藏食谱"), this);
        empty->setObjectName(QStringLiteral("FavoriteEmpty"));
        empty->setAlignment(Qt::AlignCenter);
        m_recipeRows->addWidget(empty, 1);
    } else {
        for (const Recipe &recipe : shownRecipes) {
            auto *row = new QFrame(this);
            row->setObjectName(QStringLiteral("FavoriteRecipeRow"));
            auto *layout = new QHBoxLayout(row);
            layout->setContentsMargins(6, 6, 6, 6);
            layout->setSpacing(9);
            auto *image = new QLabel(row);
            image->setFixedSize(68, 48);
            image->setPixmap(RecipeImageProvider::pixmap(recipe.name, image->size()));
            auto *info = new QVBoxLayout;
            auto *name = new QLabel(recipe.name, row);
            name->setObjectName(QStringLiteral("FavoriteRecipeName"));
            auto *meta = new QLabel(
                QStringLiteral("%1 · %2 kcal%3")
                    .arg(recipe.category).arg(recipe.totalCalories, 0, 'f', 0)
                    .arg(personalIds.contains(recipe.id) ? QStringLiteral(" · 个人食谱") : QString()),
                row);
            meta->setObjectName(QStringLiteral("FavoriteRecipeMeta"));
            info->addWidget(name);
            info->addWidget(meta);
            auto *open = new QPushButton(recipe.name, row);
            open->setProperty("class", QVariant(QStringLiteral("FavoriteNameButton")));
            connect(open, &QPushButton::clicked, this, [this, recipe]() { emit detailRequested(recipe); });
            name->hide();
            info->insertWidget(0, open);
            const bool isFavorited = favoritedIds.contains(recipe.id);
            auto *remove = new QPushButton(isFavorited ? QStringLiteral("取消收藏")
                                                       : QStringLiteral("加入收藏"), row);
            remove->setProperty("class", QVariant(QStringLiteral("LinkButton")));
            connect(remove, &QPushButton::clicked, this, [this, recipe]() {
                emit favoriteToggled(recipe.id);
                reload();
            });
            layout->addWidget(image);
            layout->addLayout(info, 1);
            layout->addWidget(remove);
            m_recipeRows->addWidget(row);
        }
        m_recipeRows->addStretch();
    }

    if (shownFoods.isEmpty()) {
        auto *foodEmpty = new QLabel(filter.isEmpty()
                                         ? QStringLiteral("暂无收藏食材\n可在食材库中点击星标收藏。")
                                         : QStringLiteral("未找到匹配的收藏食材"), this);
        foodEmpty->setObjectName(QStringLiteral("FavoriteEmpty"));
        foodEmpty->setAlignment(Qt::AlignCenter);
        m_foodRows->addWidget(foodEmpty, 1);
    } else {
        for (const Food &food : shownFoods) {
            auto *row = new QFrame(this);
            row->setObjectName(QStringLiteral("FavoriteFoodRow"));
            auto *layout = new QHBoxLayout(row);
            layout->setContentsMargins(8, 7, 8, 7);
            layout->setSpacing(10);
            auto *mark = new QLabel(food.name.left(1), row);
            mark->setObjectName(QStringLiteral("FavoriteFoodMark"));
            mark->setAlignment(Qt::AlignCenter);
            auto *info = new QVBoxLayout;
            info->setSpacing(3);
            auto *name = new QLabel(food.name, row);
            name->setObjectName(QStringLiteral("FavoriteRecipeName"));
            auto *meta = new QLabel(
                QStringLiteral("%1    蛋白质 %2 g    热量 %3 kcal")
                    .arg(food.category.isEmpty() ? QStringLiteral("其他") : food.category)
                    .arg(food.protein, 0, 'f', 1)
                    .arg(food.calories, 0, 'f', 0),
                row);
            meta->setObjectName(QStringLiteral("FavoriteRecipeMeta"));
            info->addWidget(name);
            info->addWidget(meta);
            auto *remove = new QPushButton(QStringLiteral("取消收藏"), row);
            remove->setProperty("class", QVariant(QStringLiteral("LinkButton")));
            connect(remove, &QPushButton::clicked, this, [this, food]() {
                emit foodFavoriteToggled(food.id);
            });
            layout->addWidget(mark);
            layout->addLayout(info, 1);
            layout->addWidget(remove);
            m_foodRows->addWidget(row);
        }
        m_foodRows->addStretch();
    }
}
