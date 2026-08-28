#include "RecipeDetailDialog.h"

#include "../dao/RecipeDAO.h"
#include "../services/RecipeText.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QtGlobal>
#include <QVBoxLayout>

namespace {
Recipe loadFullRecipe(const Recipe &recipe)
{
    Recipe full = recipe;
    if (full.id <= 0)
        return full;
    RecipeDAO dao;
    if (full.ingredients.isEmpty())
        full.ingredients = dao.getIngredients(full.id);
    if (full.steps.isEmpty()) {
        const Recipe loaded = dao.findById(full.id);
        if (loaded.isValid())
            full = loaded;
    }
    full.name = RecipeText::normalizeName(full.name);
    return full;
}

void addDishSection(QVBoxLayout *layout, QWidget *parent, const Recipe &recipe, int index, int total)
{
    auto *section = new QFrame(parent);
    section->setProperty("class", QVariant(QStringLiteral("PanelCard")));
    auto *secLay = new QVBoxLayout(section);
    secLay->setContentsMargins(14, 12, 14, 12);
    secLay->setSpacing(8);

    if (total > 1) {
        auto *badge = new QLabel(QStringLiteral("第 %1 道").arg(index + 1), section);
        badge->setProperty("class", QVariant(QStringLiteral("HintText")));
        secLay->addWidget(badge);
    }

    auto *dishTitle = new QLabel(recipe.name, section);
    dishTitle->setProperty("class", QVariant(QStringLiteral("SectionTitle")));
    secLay->addWidget(dishTitle);

    auto *dishMeta = new QLabel(
        QStringLiteral("%1 kcal · 约 %2 分钟")
            .arg(static_cast<int>(qRound(recipe.totalCalories)))
            .arg(recipe.cookMinutes > 0 ? recipe.cookMinutes : 15),
        section);
    dishMeta->setProperty("class", QVariant(QStringLiteral("HintText")));
    secLay->addWidget(dishMeta);

    if (recipe.totalWeight > 0.0) {
        auto *basis = new QLabel(
            QStringLiteral("每100g：%1 kcal · 蛋白质 %2g · 脂肪 %3g · 碳水 %4g；整份约 %5g")
                .arg(recipe.per100Calories, 0, 'f', 1)
                .arg(recipe.per100Protein, 0, 'f', 1)
                .arg(recipe.per100Fat, 0, 'f', 1)
                .arg(recipe.per100Carbs, 0, 'f', 1)
                .arg(recipe.totalWeight, 0, 'f', 0),
            section);
        basis->setWordWrap(true);
        basis->setProperty("class", QVariant(QStringLiteral("HintText")));
        secLay->addWidget(basis);
    }

    auto *ingTitle = new QLabel(QStringLiteral("食材"), section);
    ingTitle->setProperty("class", QVariant(QStringLiteral("HintText")));
    secLay->addWidget(ingTitle);

    if (recipe.ingredients.isEmpty()) {
        secLay->addWidget(new QLabel(QStringLiteral("暂无食材明细"), section));
    } else {
        for (const RecipeIngredient &ing : recipe.ingredients) {
            auto *row = new QLabel(
                QStringLiteral("· %1  %2 g").arg(ing.foodName).arg(ing.quantity, 0, 'f', 0),
                section);
            row->setProperty("class", QVariant(QStringLiteral("HintText")));
            secLay->addWidget(row);
        }
    }

    auto *stepsTitle = new QLabel(QStringLiteral("步骤"), section);
    stepsTitle->setProperty("class", QVariant(QStringLiteral("HintText")));
    secLay->addWidget(stepsTitle);

    QString steps = recipe.steps.trimmed();
    if (steps.isEmpty())
        steps = QStringLiteral("暂无步骤说明。");
    auto *stepsLabel = new QLabel(steps, section);
    stepsLabel->setWordWrap(true);
    stepsLabel->setTextFormat(Qt::PlainText);
    stepsLabel->setProperty("class", QVariant(QStringLiteral("HintText")));
    secLay->addWidget(stepsLabel);

    layout->addWidget(section);
}
} // namespace

RecipeDetailDialog::RecipeDetailDialog(const Recipe &recipe, QWidget *parent)
    : QDialog(parent)
{
    MealSlot slot;
    slot.mealLabel = recipe.category.isEmpty() ? QStringLiteral("餐次") : recipe.category;
    if (recipe.isValid())
        slot.dishes.append(loadFullRecipe(recipe));
    initForMeal(slot);
}

RecipeDetailDialog::RecipeDetailDialog(const MealSlot &meal, QWidget *parent)
    : QDialog(parent)
{
    initForMeal(meal);
}

void RecipeDetailDialog::initForMeal(const MealSlot &meal)
{
    QList<Recipe> dishes;
    for (const Recipe &r : meal.dishes) {
        if (r.isValid())
            dishes.append(loadFullRecipe(r));
    }

    setWindowTitle(meal.isValid()
                       ? QStringLiteral("食谱详情 · %1").arg(meal.title())
                       : QStringLiteral("食谱详情"));
    setModal(true);
    resize(540, 680);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 16);
    root->setSpacing(12);

    auto *title = new QLabel(meal.isValid() ? meal.title() : QStringLiteral("暂无菜品"), this);
    title->setObjectName(QStringLiteral("DialogTitle"));

    auto *meta = new QLabel(
        QStringLiteral("%1 · %2 kcal · 约 %3 分钟")
            .arg(meal.mealLabel)
            .arg(static_cast<int>(qRound(meal.totalCalories())))
            .arg(meal.cookMinutes()),
        this);
    meta->setProperty("class", QVariant(QStringLiteral("HintText")));

    auto *nutriFrame = new QFrame(this);
    nutriFrame->setProperty("class", QVariant(QStringLiteral("PanelCard")));
    auto *nutriLayout = new QHBoxLayout(nutriFrame);
    auto addNutri = [&](const QString &label, const QString &value) {
        auto *box = new QVBoxLayout;
        auto *v = new QLabel(value, nutriFrame);
        v->setProperty("class", QVariant(QStringLiteral("MetricValue")));
        v->setAlignment(Qt::AlignCenter);
        auto *l = new QLabel(label, nutriFrame);
        l->setProperty("class", QVariant(QStringLiteral("MetricLabel")));
        l->setAlignment(Qt::AlignCenter);
        box->addWidget(v);
        box->addWidget(l);
        nutriLayout->addLayout(box);
    };
    addNutri(QStringLiteral("热量"), QString::number(static_cast<int>(qRound(meal.totalCalories()))));
    addNutri(QStringLiteral("蛋白 g"), QString::number(meal.totalProtein(), 'f', 1));
    addNutri(QStringLiteral("碳水 g"), QString::number(meal.totalCarbs(), 'f', 1));
    addNutri(QStringLiteral("脂肪 g"), QString::number(meal.totalFat(), 'f', 1));

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *scrollBody = new QWidget;
    auto *scrollLayout = new QVBoxLayout(scrollBody);
    scrollLayout->setSpacing(12);

    if (dishes.isEmpty()) {
        scrollLayout->addWidget(new QLabel(QStringLiteral("暂无菜品详情"), scrollBody));
    } else {
        const int total = dishes.size();
        for (int i = 0; i < total; ++i)
            addDishSection(scrollLayout, scrollBody, dishes.at(i), i, total);
    }
    scrollLayout->addStretch();
    scroll->setWidget(scrollBody);

    auto *disclaimer = new QLabel(
        QStringLiteral("免责声明：本推荐仅供学习与日常参考，不能替代专业医疗或营养诊疗建议。"),
        this);
    disclaimer->setObjectName(QStringLiteral("Disclaimer"));
    disclaimer->setWordWrap(true);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭"));
    buttons->button(QDialogButtonBox::Close)->setProperty("class", QVariant(QStringLiteral("PrimaryButton")));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &QDialog::accept);

    root->addWidget(title);
    root->addWidget(meta);
    root->addWidget(nutriFrame);
    root->addWidget(scroll, 1);
    root->addWidget(disclaimer);
    root->addWidget(buttons);
}
