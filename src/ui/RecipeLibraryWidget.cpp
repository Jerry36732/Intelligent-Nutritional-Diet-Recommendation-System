#include "RecipeLibraryWidget.h"

#include "RecipeEditorDialog.h"
#include "UiAssets.h"
#include "../dao/RecipeDAO.h"
#include "../services/RecipeImageProvider.h"

#include <QButtonGroup>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

RecipeLibraryWidget::RecipeLibraryWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("RecipeLibraryWidget"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 8, 28, 18);
    root->setSpacing(10);

    auto *toolbar = new QHBoxLayout;
    toolbar->setSpacing(10);
    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("RecipeLibrarySearch"));
    m_search->setPlaceholderText(QStringLiteral("搜索菜名或食材"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(UiAssets::svgIcon(QStringLiteral("search")), QLineEdit::LeadingPosition);
    m_count = new QLabel(QStringLiteral("共 0 道食谱"), this);
    m_count->setObjectName(QStringLiteral("RecipeLibraryCount"));
    auto *webImport = new QPushButton(QStringLiteral("网页导入"), this);
    webImport->setProperty("class", QStringLiteral("GhostButton"));
    UiAssets::setButtonIcon(webImport, QStringLiteral("refresh"), 17);
    auto *manualCreate = new QPushButton(QStringLiteral("手动创建"), this);
    manualCreate->setProperty("class", QStringLiteral("PrimaryButton"));
    UiAssets::setButtonIcon(manualCreate, QStringLiteral("plus"), 17, QColor(Qt::white));
    toolbar->addWidget(m_search, 1);
    toolbar->addWidget(m_count);
    toolbar->addWidget(webImport);
    toolbar->addWidget(manualCreate);
    root->addLayout(toolbar);

    auto *filterHost = new QFrame(this);
    filterHost->setObjectName(QStringLiteral("RecipeLibraryFilters"));
    auto *filterLayout = new QHBoxLayout(filterHost);
    filterLayout->setContentsMargins(0, 0, 0, 0);
    filterLayout->setSpacing(4);
    m_filters = new QButtonGroup(this);
    m_filters->setExclusive(true);
    const QList<QPair<QString, QString>> filters = {
        {QStringLiteral("全部"), QString()},
        {QStringLiteral("荤菜"), QStringLiteral("meat")},
        {QStringLiteral("素菜"), QStringLiteral("vegetable")},
        {QStringLiteral("主食"), QStringLiteral("staple")},
        {QStringLiteral("汤羹"), QStringLiteral("soup")},
        {QStringLiteral("甜品"), QStringLiteral("dessert")},
    };
    for (int i = 0; i < filters.size(); ++i) {
        auto *button = new QPushButton(filters.at(i).first, filterHost);
        button->setCheckable(true);
        button->setProperty("class", QStringLiteral("RecipeLibraryFilter"));
        button->setProperty("filterKey", filters.at(i).second);
        button->setCursor(Qt::PointingHandCursor);
        button->setChecked(i == 0);
        m_filters->addButton(button, i);
        filterLayout->addWidget(button, 1);
    }
    root->addWidget(filterHost);

    auto *cardsHost = new QWidget(this);
    cardsHost->setObjectName(QStringLiteral("RecipeLibraryCards"));
    m_grid = new QGridLayout(cardsHost);
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(10);
    m_grid->setVerticalSpacing(10);
    for (int column = 0; column < 3; ++column)
        m_grid->setColumnStretch(column, 1);
    for (int row = 0; row < 2; ++row)
        m_grid->setRowStretch(row, 1);
    root->addWidget(cardsHost, 1);

    auto *pager = new QHBoxLayout;
    pager->setSpacing(8);
    m_prev = new QPushButton(QStringLiteral("上一页"), this);
    m_next = new QPushButton(QStringLiteral("下一页"), this);
    for (QPushButton *button : {m_prev, m_next}) {
        button->setProperty("class", QStringLiteral("GhostButton"));
        button->setCursor(Qt::PointingHandCursor);
    }
    m_pageLabel = new QLabel(QStringLiteral("第 1 / 1 页"), this);
    m_pageLabel->setObjectName(QStringLiteral("RecipeLibraryPageLabel"));
    pager->addStretch();
    pager->addWidget(m_prev);
    pager->addWidget(m_pageLabel);
    pager->addWidget(m_next);
    pager->addStretch();
    root->addLayout(pager);

    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(180);
    connect(m_search, &QLineEdit::textChanged, this, [this]() {
        m_page = 0;
        m_searchTimer->start();
    });
    connect(m_searchTimer, &QTimer::timeout, this, &RecipeLibraryWidget::rebuild);
    connect(m_filters, &QButtonGroup::idClicked, this, [this](int id) {
        if (QAbstractButton *button = m_filters->button(id))
            setFilter(button->property("filterKey").toString());
    });
    connect(m_prev, &QPushButton::clicked, this, [this]() {
        if (m_page > 0) { --m_page; rebuild(); }
    });
    connect(m_next, &QPushButton::clicked, this, [this]() {
        if ((m_page + 1) * PageSize < m_total) { ++m_page; rebuild(); }
    });
    connect(webImport, &QPushButton::clicked, this, &RecipeLibraryWidget::openWebImporter);
    connect(manualCreate, &QPushButton::clicked, this, &RecipeLibraryWidget::openManualCreator);
}

void RecipeLibraryWidget::setUser(const User &user)
{
    m_user = user;
    m_page = 0;
}

void RecipeLibraryWidget::reload()
{
    rebuild();
}

void RecipeLibraryWidget::setFilter(const QString &key)
{
    m_filterKey = key;
    m_page = 0;
    rebuild();
}

void RecipeLibraryWidget::clearCards()
{
    while (QLayoutItem *item = m_grid->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

QString RecipeLibraryWidget::roleLabel(const Recipe &recipe) const
{
    if (recipe.dishRole == QLatin1String("staple")) return QStringLiteral("主食");
    if (recipe.dishRole == QLatin1String("meat")) return QStringLiteral("荤菜");
    if (recipe.dishRole == QLatin1String("vegetable")) return QStringLiteral("素菜");
    if (recipe.dishRole == QLatin1String("soup")) return QStringLiteral("汤羹");
    if (recipe.dishRole == QLatin1String("dessert")
        || recipe.name.contains(QStringLiteral("蛋糕"))
        || recipe.name.contains(QStringLiteral("甜品"))
        || recipe.name.contains(QStringLiteral("糖水"))
        || recipe.name.contains(QStringLiteral("布丁")))
        return QStringLiteral("甜品");
    if (recipe.dishRole == QLatin1String("drink")) return QStringLiteral("饮品");
    return recipe.category;
}

QWidget *RecipeLibraryWidget::createRecipeCard(const Recipe &recipe)
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("RecipeLibraryCard"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(0, 0, 0, 9);
    layout->setSpacing(5);

    auto *imageHost = new QFrame(card);
    imageHost->setObjectName(QStringLiteral("RecipeLibraryImageHost"));
    imageHost->setFixedHeight(105);
    auto *imageGrid = new QGridLayout(imageHost);
    imageGrid->setContentsMargins(0, 0, 0, 0);
    auto *image = new QPushButton(imageHost);
    image->setObjectName(QStringLiteral("RecipeLibraryImage"));
    image->setCursor(Qt::PointingHandCursor);
    image->setIcon(QIcon(RecipeImageProvider::pixmap(recipe.name, QSize(260, 110))));
    image->setIconSize(QSize(260, 110));
    auto *favorite = new QPushButton(imageHost);
    favorite->setObjectName(QStringLiteral("RecipeLibraryFavorite"));
    favorite->setCheckable(true);
    favorite->setChecked(m_user.id > 0 && RecipeDAO().isFavorite(m_user.id, recipe.id));
    favorite->setCursor(Qt::PointingHandCursor);
    favorite->setToolTip(favorite->isChecked() ? QStringLiteral("取消收藏") : QStringLiteral("收藏食谱"));
    UiAssets::setButtonIcon(favorite, favorite->isChecked() ? QStringLiteral("star-filled")
                                                             : QStringLiteral("star-outline"),
                            20, QColor(QStringLiteral("#087A53")));
    imageGrid->addWidget(image, 0, 0);
    imageGrid->addWidget(favorite, 0, 0, Qt::AlignRight | Qt::AlignTop);
    layout->addWidget(imageHost);

    auto *name = new QPushButton(recipe.name, card);
    name->setObjectName(QStringLiteral("RecipeLibraryName"));
    name->setCursor(Qt::PointingHandCursor);
    auto *meta = new QLabel(QStringLiteral("%1 · %2 kcal · %3 分钟")
                                .arg(roleLabel(recipe))
                                .arg(recipe.totalCalories, 0, 'f', 0)
                                .arg(qMax(1, recipe.cookMinutes)), card);
    meta->setObjectName(QStringLiteral("RecipeLibraryMeta"));
    layout->addWidget(name);
    layout->addWidget(meta);

    auto openRecipe = [this, recipe]() { emit detailRequested(recipe); };
    connect(image, &QPushButton::clicked, this, openRecipe);
    connect(name, &QPushButton::clicked, this, openRecipe);
    connect(favorite, &QPushButton::clicked, this, [this, recipe, favorite](bool checked) {
        UiAssets::setButtonIcon(favorite, checked ? QStringLiteral("star-filled")
                                                  : QStringLiteral("star-outline"),
                                20, QColor(QStringLiteral("#087A53")));
        favorite->setToolTip(checked ? QStringLiteral("取消收藏") : QStringLiteral("收藏食谱"));
        emit favoriteToggled(recipe.id);
    });
    return card;
}

void RecipeLibraryWidget::rebuild()
{
    clearCards();
    RecipeDAO dao;
    const QList<Recipe> recipes = dao.browse(m_search ? m_search->text() : QString(),
                                              m_filterKey, m_user.id,
                                              m_page * PageSize, PageSize, &m_total);
    const int pages = qMax(1, (m_total + PageSize - 1) / PageSize);
    if (m_page >= pages) {
        m_page = pages - 1;
        return rebuild();
    }
    m_count->setText(QStringLiteral("共 %1 道食谱").arg(m_total));
    m_pageLabel->setText(QStringLiteral("第 %1 / %2 页").arg(m_page + 1).arg(pages));
    m_prev->setEnabled(m_page > 0);
    m_next->setEnabled(m_page + 1 < pages);
    if (recipes.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("未找到符合条件的食谱\n可以更换分类或手动创建自己的食谱。"), this);
        empty->setObjectName(QStringLiteral("RecipeLibraryEmpty"));
        empty->setAlignment(Qt::AlignCenter);
        m_grid->addWidget(empty, 0, 0, 2, 3);
        return;
    }
    for (int i = 0; i < recipes.size(); ++i)
        m_grid->addWidget(createRecipeCard(recipes.at(i)), i / 3, i % 3);
}

void RecipeLibraryWidget::openWebImporter()
{
    if (m_user.id <= 0) return;
    RecipeEditorDialog dialog(m_user.id, RecipeEditorDialog::Mode::WebImport, this);
    connect(&dialog, &RecipeEditorDialog::recipeCreated, this, [this](int recipeId) {
        rebuild();
        emit personalRecipeCreated(recipeId);
    });
    dialog.exec();
}

void RecipeLibraryWidget::openManualCreator()
{
    if (m_user.id <= 0) return;
    RecipeEditorDialog dialog(m_user.id, RecipeEditorDialog::Mode::Manual, this);
    connect(&dialog, &RecipeEditorDialog::recipeCreated, this, [this](int recipeId) {
        rebuild();
        emit personalRecipeCreated(recipeId);
    });
    dialog.exec();
}
