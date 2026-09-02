#include "RecipeDetailDialog.h"
#include "RecipeDnaDialog.h"
#include "UiAssets.h"

#include "../dao/RecipeDAO.h"
#include "../services/RecipeImageProvider.h"
#include "../services/RecipeText.h"

#include <QApplication>
#include <QClipboard>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QShowEvent>
#include <QTabWidget>
#include <QtGlobal>
#include <QVBoxLayout>

namespace {
Recipe loadFullRecipe(const Recipe &recipe)
{
    Recipe full = recipe;
    if (full.id > 0) {
        RecipeDAO dao;
        const Recipe loaded = dao.findById(full.id);
        if (loaded.isValid())
            full = loaded;
        if (full.ingredients.isEmpty())
            full.ingredients = dao.getIngredients(full.id);
    }
    full.name = RecipeText::normalizeName(full.name);
    return full;
}

QString normalizedSteps(QString steps)
{
    steps = steps.trimmed();
    if (steps.isEmpty())
        return QStringLiteral("1. 暂无详细步骤说明。");
    steps.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    steps.replace(QChar::CarriageReturn, QChar::LineFeed);
    return steps;
}

QStringList splitSteps(QString text)
{
    text = normalizedSteps(text);

    // 只在明确出现下一步编号时换行。普通句号属于当前步骤正文，不能拆成新步骤。
    // 同时兼容少量 MDB 数据把“2、”紧接在上一句末尾的情况。
    text.replace(
        QRegularExpression(
            QStringLiteral("([。；;.!！?？,，]\\s*)(?=\\d+[.、。．)）]\\s*)")),
        QStringLiteral("\\1\n"));
    text.replace(
        QRegularExpression(QStringLiteral("[ \\t]+(?=\\d+[.、。．)）]\\s*)")),
        QStringLiteral("\n"));

    QStringList result;
    const QStringList parts = text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                                         Qt::SkipEmptyParts);
    for (QString part : parts) {
        // 连续清理重复源编号，例如历史数据中的“1. 1。步骤正文”。
        part.remove(QRegularExpression(
            QStringLiteral("^\\s*(?:\\d+[.、。．)）]\\s*)+")));
        part = part.trimmed();
        if (!part.isEmpty())
            result.append(part);
    }
    if (result.isEmpty())
        result.append(QStringLiteral("暂无详细步骤说明"));
    return result;
}

QWidget *createDishPage(const Recipe &recipe, QWidget *parent)
{
    auto *page = new QWidget(parent);
    page->setObjectName(QStringLiteral("RecipeDetailPage"));
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(7, 7, 7, 0);
    layout->setSpacing(18);

    auto *ingredientColumn = new QWidget(page);
    auto *ingredients = new QVBoxLayout(ingredientColumn);
    ingredients->setContentsMargins(0, 0, 0, 0);
    ingredients->setSpacing(4);
    auto *ingredientTitle = new QLabel(QStringLiteral("食材清单（1人份）"), ingredientColumn);
    ingredientTitle->setObjectName(QStringLiteral("RecipeColumnTitle"));
    ingredients->addWidget(ingredientTitle);

    auto *ingredientScroll = new QScrollArea(ingredientColumn);
    ingredientScroll->setObjectName(QStringLiteral("RecipeIngredientsScroll"));
    ingredientScroll->setWidgetResizable(true);
    ingredientScroll->setFrameShape(QFrame::NoFrame);
    ingredientScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ingredientScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    auto *ingredientContent = new QWidget(ingredientScroll);
    ingredientContent->setObjectName(QStringLiteral("RecipeIngredientsContent"));
    auto *ingredientRows = new QVBoxLayout(ingredientContent);
    ingredientRows->setContentsMargins(0, 0, 4, 0);
    ingredientRows->setSpacing(4);
    if (recipe.ingredients.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("暂无食材明细"), ingredientContent);
        empty->setProperty("class", QStringLiteral("HintText"));
        ingredientRows->addWidget(empty);
    } else {
        for (int index = 0; index < recipe.ingredients.size(); ++index) {
            const RecipeIngredient &ingredient = recipe.ingredients.at(index);
            auto *row = new QHBoxLayout;
            row->setSpacing(7);
            auto *dot = new QFrame(ingredientContent);
            dot->setObjectName(QStringLiteral("RecipeDetailBulletDot"));
            dot->setFixedSize(4, 4);
            auto *name = new QLabel(ingredient.foodName, ingredientContent);
            name->setObjectName(QStringLiteral("RecipeIngredientLine"));
            const QString quantityDisplay = ingredient.quantityText.trimmed().isEmpty()
                ? QStringLiteral("%1 g").arg(ingredient.quantity, 0, 'f', 0)
                : ingredient.quantityText;
            auto *quantity = new QLabel(quantityDisplay, ingredientContent);
            quantity->setObjectName(QStringLiteral("RecipeIngredientQuantity"));
            row->addWidget(dot, 0, Qt::AlignVCenter);
            row->addWidget(name);
            row->addStretch();
            row->addWidget(quantity);
            ingredientRows->addLayout(row);
        }
    }
    ingredientRows->addStretch();
    ingredientScroll->setWidget(ingredientContent);
    ingredients->addWidget(ingredientScroll, 1);

    auto *divider = new QFrame(page);
    divider->setObjectName(QStringLiteral("RecipeDetailColumnRule"));
    divider->setFixedWidth(1);

    auto *steps = new QVBoxLayout;
    steps->setSpacing(5);
    auto *stepsTitle = new QLabel(QStringLiteral("制作步骤"), page);
    stepsTitle->setObjectName(QStringLiteral("RecipeColumnTitle"));
    steps->addWidget(stepsTitle);

    auto *stepsScroll = new QScrollArea(page);
    stepsScroll->setObjectName(QStringLiteral("RecipeStepsScroll"));
    stepsScroll->setWidgetResizable(true);
    stepsScroll->setFrameShape(QFrame::NoFrame);
    stepsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    stepsScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    auto *stepsContent = new QWidget(stepsScroll);
    stepsContent->setObjectName(QStringLiteral("RecipeStepsContent"));
    auto *stepRows = new QVBoxLayout(stepsContent);
    stepRows->setContentsMargins(0, 0, 4, 0);
    stepRows->setSpacing(5);

    const QStringList stepItems = splitSteps(recipe.steps);
    for (int index = 0; index < stepItems.size(); ++index) {
        auto *row = new QHBoxLayout;
        row->setSpacing(8);
        auto *number = new QLabel(QString::number(index + 1), stepsContent);
        number->setObjectName(QStringLiteral("RecipeStepNumber"));
        number->setProperty("tone", index % 3 == 0 ? QStringLiteral("green")
                                   : index % 3 == 1 ? QStringLiteral("blue")
                                                    : QStringLiteral("orange"));
        number->setFixedSize(18, 18);
        number->setAlignment(Qt::AlignCenter);
        auto *line = new QLabel(stepItems.at(index), stepsContent);
        line->setObjectName(QStringLiteral("RecipeStepsText"));
        line->setWordWrap(true);
        row->addWidget(number, 0, Qt::AlignTop);
        row->addWidget(line, 1);
        stepRows->addLayout(row);
    }
    stepRows->addStretch();
    stepsScroll->setWidget(stepsContent);
    steps->addWidget(stepsScroll, 1);

    layout->addWidget(ingredientColumn, 7);
    layout->addWidget(divider);
    layout->addLayout(steps, 13);
    return page;
}
} // namespace

RecipeDetailDialog::RecipeDetailDialog(const Recipe &recipe, int userId, QWidget *parent,
                                       int reviewFavoriteState)
    : QDialog(parent)
    , m_userId(userId)
    , m_reviewFavoriteState(reviewFavoriteState)
{
    MealSlot meal;
    meal.mealLabel = recipe.category.isEmpty() ? QStringLiteral("餐次") : recipe.category;
    if (recipe.isValid())
        meal.dishes.append(recipe);
    initForMeal(meal);
}

RecipeDetailDialog::RecipeDetailDialog(const MealSlot &meal, int userId, QWidget *parent,
                                       int reviewFavoriteState)
    : QDialog(parent)
    , m_userId(userId)
    , m_reviewFavoriteState(reviewFavoriteState)
{
    initForMeal(meal);
}

void RecipeDetailDialog::initForMeal(const MealSlot &meal)
{
    QList<Recipe> dishes;
    for (const Recipe &recipe : meal.dishes) {
        if (recipe.isValid())
            dishes.append(loadFullRecipe(recipe));
    }
    const Recipe primary = dishes.isEmpty() ? Recipe{} : dishes.first();

    setWindowTitle(QStringLiteral("食谱详情"));
    setModal(true);
    setWindowFlag(Qt::FramelessWindowHint, true);
    setObjectName(QStringLiteral("RecipeDetailDialog"));
    setFixedSize(720, 700);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(30, 22, 30, 24);
    root->setSpacing(0);

    auto *dialogHeader = new QHBoxLayout;
    auto *dialogCaption = new QLabel(QStringLiteral("食谱详情"), this);
    dialogCaption->setObjectName(QStringLiteral("DialogCaption"));
    dialogCaption->setFont(UiAssets::titleFont(27));
    auto *headerClose = new QPushButton(this);
    headerClose->setObjectName(QStringLiteral("DialogCloseButton"));
    headerClose->setFixedSize(38, 38);
    UiAssets::setButtonIcon(headerClose, QStringLiteral("close"), 18);
    dialogHeader->addWidget(dialogCaption);
    dialogHeader->addStretch();
    dialogHeader->addWidget(headerClose);

    auto *dishHeader = new QHBoxLayout;
    dishHeader->setSpacing(22);
    auto *image = new QLabel(this);
    image->setObjectName(QStringLiteral("RecipeDetailHeroImage"));
    image->setFixedSize(210, 150);
    if (primary.isValid())
        image->setPixmap(RecipeImageProvider::pixmap(primary.name, image->size()));
    auto *dishText = new QVBoxLayout;
    dishText->setSpacing(8);
    auto *title = new QLabel(primary.isValid() ? primary.name : QStringLiteral("暂无菜品"), this);
    title->setObjectName(QStringLiteral("RecipeDetailTitle"));
    title->setWordWrap(true);
    auto *description = new QLabel(
        primary.isValid()
            ? QStringLiteral("%1搭配，口味清爽，营养均衡，适合日常膳食。")
                  .arg(primary.category.isEmpty() ? QStringLiteral("家常") : primary.category)
            : QStringLiteral("暂无食谱说明"),
        this);
    description->setObjectName(QStringLiteral("RecipeDetailDescription"));
    description->setWordWrap(true);
    dishText->addWidget(title);
    dishText->addWidget(description);
    dishText->addStretch();
    dishHeader->addWidget(image);
    dishHeader->addLayout(dishText, 1);

    auto *nutriFrame = new QFrame(this);
    nutriFrame->setObjectName(QStringLiteral("RecipeNutritionStrip"));
    nutriFrame->setFixedHeight(92);
    auto *nutriLayout = new QHBoxLayout(nutriFrame);
    nutriLayout->setContentsMargins(8, 0, 8, 0);
    nutriLayout->setSpacing(24);
    auto addNutri = [&](QString value, const QString &label, const QString &tone,
                        bool primaryValue = false) {
        auto *metric = new QFrame(nutriFrame);
        metric->setProperty("class", QStringLiteral("RecipeNutritionMetric"));
        metric->setProperty("tone", tone);
        auto *box = new QVBoxLayout(metric);
        box->setContentsMargins(7, 5, 7, 5);
        box->setSpacing(1);
        const QString unit = value.endsWith(QStringLiteral("kcal"))
            ? QStringLiteral("kcal") : QStringLiteral("g");
        value.remove(QStringLiteral(" kcal"));
        value.remove(QStringLiteral(" g"));
        if (!primaryValue) {
            auto *caption = new QLabel(label, metric);
            caption->setObjectName(QStringLiteral("RecipeMacroLabel"));
            caption->setProperty("tone", tone);
            caption->setAlignment(Qt::AlignCenter);
            box->addWidget(caption);
        }
        auto *valueRow = new QHBoxLayout;
        valueRow->setSpacing(3);
        valueRow->addStretch();
        auto *valueLabel = new QLabel(value, metric);
        valueLabel->setObjectName(primaryValue ? QStringLiteral("RecipeKcalValue")
                                               : QStringLiteral("RecipeMacroValue"));
        auto *unitLabel = new QLabel(unit, metric);
        unitLabel->setObjectName(QStringLiteral("RecipeMetricUnit"));
        valueRow->addWidget(valueLabel, 0, Qt::AlignBaseline);
        valueRow->addWidget(unitLabel, 0, Qt::AlignBaseline);
        valueRow->addStretch();
        box->addLayout(valueRow);
        nutriLayout->addWidget(metric, 1);
    };
    addNutri(QStringLiteral("%1 kcal").arg(static_cast<int>(qRound(meal.totalCalories()))),
             QStringLiteral("热量"), QStringLiteral("kcal"), true);
    addNutri(QStringLiteral("%1 g").arg(meal.totalProtein(), 0, 'f', 1),
             QStringLiteral("蛋白质"), QStringLiteral("protein"));
    addNutri(QStringLiteral("%1 g").arg(meal.totalCarbs(), 0, 'f', 1),
             QStringLiteral("碳水"), QStringLiteral("carbs"));
    addNutri(QStringLiteral("%1 g").arg(meal.totalFat(), 0, 'f', 1),
             QStringLiteral("脂肪"), QStringLiteral("fat"));

    QWidget *detailBody = nullptr;
    if (dishes.size() <= 1) {
        detailBody = dishes.isEmpty() ? new QWidget(this) : createDishPage(dishes.first(), this);
    } else {
        auto *tabs = new QTabWidget(this);
        tabs->setObjectName(QStringLiteral("RecipeDishTabs"));
        for (const Recipe &dish : dishes)
            tabs->addTab(createDishPage(dish, tabs), dish.name);
        detailBody = tabs;
    }

    auto *actions = new QHBoxLayout;
    auto *favorite = new QPushButton(QStringLiteral("收藏食谱"), this);
    favorite->setObjectName(QStringLiteral("RecipeFavoriteButton"));
    favorite->setProperty("class", QStringLiteral("GhostButton"));
    favorite->setFixedSize(174, 46);
    auto *close = new QPushButton(QStringLiteral("关闭"), this);
    close->setObjectName(QStringLiteral("RecipeCloseButton"));
    close->setProperty("class", QStringLiteral("GhostButton"));
    close->setFixedSize(104, 46);
    auto *share = new QPushButton(QStringLiteral("复制分享"), this);
    share->setObjectName(QStringLiteral("RecipeShareButton"));
    share->setProperty("class", QStringLiteral("GhostButton"));
    share->setFixedSize(126, 46);
    UiAssets::setButtonIcon(share, QStringLiteral("send"), 17);
    auto *dna = new QPushButton(QStringLiteral("DNA改造"), this);
    dna->setObjectName(QStringLiteral("RecipeDnaButton"));
    dna->setProperty("class", QStringLiteral("GhostButton"));
    dna->setFixedSize(120, 46);
    dna->setEnabled(primary.isValid() && m_userId > 0);
    UiAssets::setButtonIcon(dna, QStringLiteral("dna"), 17,
                            QColor(QStringLiteral("#08A96E")));
    const bool initiallyFavorited = m_reviewFavoriteState >= 0
        ? (m_reviewFavoriteState != 0)
        : (primary.isValid() && m_userId > 0 && RecipeDAO().isFavorite(m_userId, primary.id));
    favorite->setCheckable(true);
    favorite->setChecked(initiallyFavorited);
    auto updateFavoriteIcon = [favorite](bool selected) {
        favorite->setText(selected ? QStringLiteral("取消收藏") : QStringLiteral("收藏食谱"));
        UiAssets::setButtonIcon(favorite,
                                selected ? QStringLiteral("star-filled")
                                         : QStringLiteral("star-outline"),
                                22, selected ? QColor(QStringLiteral("#08A96E"))
                                            : QColor(QStringLiteral("#08A96E")));
    };
    updateFavoriteIcon(initiallyFavorited);
    favorite->setEnabled(primary.isValid() && m_userId > 0);
    auto *favoriteBox = new QVBoxLayout;
    favoriteBox->setSpacing(4);
    auto *favoriteHint = new QLabel(this);
    favoriteHint->setObjectName(QStringLiteral("RecipeFavoriteHint"));
    favoriteHint->setText(initiallyFavorited
                              ? QStringLiteral("已加入我的收藏，再次点击可取消收藏")
                              : QStringLiteral("点击收藏后星标填充，并加入我的收藏"));
    favoriteBox->addWidget(favorite);
    favoriteBox->addWidget(favoriteHint);
    actions->addLayout(favoriteBox);
    actions->addStretch();
    actions->addWidget(dna, 0, Qt::AlignTop);
    actions->addWidget(share, 0, Qt::AlignTop);
    actions->addWidget(close);

    connect(headerClose, &QPushButton::clicked, this, &QDialog::accept);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(share, &QPushButton::clicked, this, [this, dishes]() {
        QString text = QStringLiteral("膳衡食谱分享\n");
        for (const Recipe &summary : dishes) {
            const Recipe recipe = loadFullRecipe(summary);
            text += QStringLiteral("\n【%1】\n热量：%2 kcal\n")
                        .arg(recipe.name).arg(recipe.totalCalories, 0, 'f', 0);
            text += QStringLiteral("原料：\n");
            for (const RecipeIngredient &ingredient : recipe.ingredients)
                text += QStringLiteral("- %1 %2\n").arg(ingredient.foodName,
                    ingredient.quantityText.trimmed().isEmpty()
                        ? QStringLiteral("%1 g").arg(ingredient.quantity, 0, 'f', 1)
                        : ingredient.quantityText);
            text += QStringLiteral("制作步骤：\n%1\n").arg(normalizedSteps(recipe.steps));
        }
        QApplication::clipboard()->setText(text);
        QMessageBox::information(this, QStringLiteral("已复制"),
                                 QStringLiteral("食谱已复制，可直接粘贴给家人或朋友。"));
    });
    connect(dna, &QPushButton::clicked, this, [this, primary]() {
        RecipeDnaDialog dialog(primary, m_userId, this);
        connect(&dialog, &RecipeDnaDialog::personalRecipeCreated,
                this, &RecipeDetailDialog::personalRecipeCreated);
        dialog.exec();
    });
    connect(favorite, &QPushButton::clicked, this,
            [this, favorite, favoriteHint, primary, updateFavoriteIcon](bool desired) {
        if (primary.id <= 0 || m_userId <= 0)
            return;
        RecipeDAO dao;
        if (!dao.setFavorite(m_userId, primary.id, desired)) {
            favorite->setChecked(!desired);
            updateFavoriteIcon(!desired);
            QMessageBox::warning(this, QStringLiteral("收藏失败"),
                                 QStringLiteral("收藏状态未保存，请稍后重试。"));
            return;
        }
        updateFavoriteIcon(desired);
        favoriteHint->setText(desired
                                  ? QStringLiteral("已加入我的收藏，再次点击可取消收藏")
                                  : QStringLiteral("点击收藏后星标填充，并加入我的收藏"));
        emit favoriteChanged(primary.id, desired);
    });

    root->addLayout(dialogHeader);
    root->addSpacing(19);
    root->addLayout(dishHeader);
    root->addSpacing(10);
    auto *heroRule = new QFrame(this);
    heroRule->setObjectName(QStringLiteral("RecipeDetailRule"));
    heroRule->setFixedHeight(1);
    root->addWidget(heroRule);
    root->addSpacing(13);
    root->addWidget(nutriFrame);
    root->addSpacing(13);
    auto *bodyRule = new QFrame(this);
    bodyRule->setObjectName(QStringLiteral("RecipeDetailRule"));
    bodyRule->setFixedHeight(1);
    root->addWidget(bodyRule);
    root->addWidget(detailBody, 1);
    auto *footerRule = new QFrame(this);
    footerRule->setObjectName(QStringLiteral("RecipeDetailRule"));
    footerRule->setFixedHeight(1);
    root->addWidget(footerRule);
    root->addSpacing(10);
    root->addLayout(actions);
    actions->setAlignment(close, Qt::AlignTop);
}

void RecipeDetailDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (QWidget *owner = parentWidget()) {
        const QRect parentRect = owner->frameGeometry();
        move(parentRect.center().x() - width() / 2,
             parentRect.center().y() - height() / 2);
    }
}
