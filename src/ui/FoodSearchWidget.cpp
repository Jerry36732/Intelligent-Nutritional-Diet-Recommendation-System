#include "FoodSearchWidget.h"

#include "../dao/DatabaseManager.h"
#include "../dao/FoodDAO.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
struct CategoryDefinition {
    const char *name;
    const char *detail;
};

const CategoryDefinition kCategories[] = {
    {"全部分类", "浏览全部食材"},
    {"谷类及制品", "小麦 · 稻米 · 玉米 · 杂粮"},
    {"薯类、淀粉及制品", "薯类 · 淀粉类"},
    {"干豆类及制品", "大豆 · 绿豆 · 赤豆 · 豆制品"},
    {"蔬菜类及制品", "根菜 · 鲜豆 · 瓜果 · 叶菜"},
    {"菌藻类", "菌类 · 藻类"},
    {"水果类及制品", "仁果 · 核果 · 浆果 · 瓜果"},
    {"坚果、种子类", "树坚果 · 种子"},
    {"畜肉类及制品", "猪 · 牛 · 羊 · 其它"},
    {"禽肉类及制品", "鸡 · 鸭 · 鹅 · 火鸡"},
    {"乳类及制品", "液态乳 · 酸奶 · 奶酪"},
    {"蛋类及制品", "鸡蛋 · 鸭蛋 · 鹌鹑蛋"},
    {"鱼虾蟹贝类", "鱼 · 虾 · 蟹 · 贝"},
    {"饮料类", "果汁 · 茶饮 · 含乳饮料"},
    {"油脂及调味品", "植物油 · 动物油 · 酱醋"},
};

bool containsAny(const QString &text, const QStringList &words)
{
    for (const QString &word : words) {
        if (text.contains(word, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

const QStringList &kwFungi()
{
    static const QStringList w = {QStringLiteral("香菇"), QStringLiteral("蘑菇"), QStringLiteral("木耳"),
                                  QStringLiteral("银耳"), QStringLiteral("平菇"), QStringLiteral("金针菇"),
                                  QStringLiteral("海带"), QStringLiteral("紫菜"), QStringLiteral("藻")};
    return w;
}
const QStringList &kwBean()
{
    static const QStringList w = {QStringLiteral("豆腐"), QStringLiteral("豆浆"), QStringLiteral("豆皮"),
                                  QStringLiteral("豆干"), QStringLiteral("腐竹"), QStringLiteral("豆花"),
                                  QStringLiteral("黄豆"), QStringLiteral("黑豆"), QStringLiteral("绿豆"),
                                  QStringLiteral("红豆"), QStringLiteral("豌豆"), QStringLiteral("蚕豆"),
                                  QStringLiteral("芸豆"), QStringLiteral("豆")};
    return w;
}
const QStringList &kwMilk()
{
    static const QStringList w = {QStringLiteral("牛奶"), QStringLiteral("奶粉"), QStringLiteral("酸奶"),
                                  QStringLiteral("奶酪"), QStringLiteral("乳")};
    return w;
}
const QStringList &kwPoultry()
{
    static const QStringList w = {QStringLiteral("鸡肉"), QStringLiteral("鸡胸"), QStringLiteral("鸭"),
                                  QStringLiteral("鹅"), QStringLiteral("火鸡")};
    return w;
}
const QStringList &kwMeat()
{
    static const QStringList w = {QStringLiteral("猪"), QStringLiteral("牛肉"), QStringLiteral("牛里脊"),
                                  QStringLiteral("羊"), QStringLiteral("兔"), QStringLiteral("火腿")};
    return w;
}
const QStringList &kwEgg()
{
    static const QStringList w = {QStringLiteral("鸡蛋"), QStringLiteral("鸭蛋"), QStringLiteral("鹅蛋"),
                                  QStringLiteral("鹌鹑蛋")};
    return w;
}
const QStringList &kwSeafood()
{
    static const QStringList w = {QStringLiteral("鱼"), QStringLiteral("虾"), QStringLiteral("蟹"),
                                  QStringLiteral("贝"), QStringLiteral("蛤"), QStringLiteral("牡蛎"),
                                  QStringLiteral("海参"), QStringLiteral("鱿"), QStringLiteral("鲍")};
    return w;
}
const QStringList &kwGrain()
{
    static const QStringList w = {QStringLiteral("米"), QStringLiteral("面"), QStringLiteral("麦"),
                                  QStringLiteral("燕麦"), QStringLiteral("玉米"), QStringLiteral("高粱"),
                                  QStringLiteral("荞麦"), QStringLiteral("小米"), QStringLiteral("薏"),
                                  QStringLiteral("粥"), QStringLiteral("粉")};
    return w;
}
const QStringList &kwStarch()
{
    static const QStringList w = {QStringLiteral("土豆"), QStringLiteral("红薯"), QStringLiteral("紫薯"),
                                  QStringLiteral("山药"), QStringLiteral("芋"), QStringLiteral("淀粉")};
    return w;
}
const QStringList &kwFruit()
{
    static const QStringList w = {QStringLiteral("苹果"), QStringLiteral("梨"), QStringLiteral("橙"),
                                  QStringLiteral("柑"), QStringLiteral("柚"), QStringLiteral("香蕉"),
                                  QStringLiteral("葡萄"), QStringLiteral("草莓"), QStringLiteral("桃"),
                                  QStringLiteral("李"), QStringLiteral("枣"), QStringLiteral("瓜"),
                                  QStringLiteral("果")};
    return w;
}
const QStringList &kwNut()
{
    static const QStringList w = {QStringLiteral("核桃"), QStringLiteral("花生"), QStringLiteral("芝麻"),
                                  QStringLiteral("杏仁"), QStringLiteral("腰果"), QStringLiteral("松子"),
                                  QStringLiteral("葵花")};
    return w;
}
const QStringList &kwSeasoning()
{
    static const QStringList w = {QStringLiteral("油"), QStringLiteral("盐"), QStringLiteral("酱油"),
                                  QStringLiteral("醋"), QStringLiteral("酱"), QStringLiteral("糖"),
                                  QStringLiteral("辣椒"), QStringLiteral("胡椒"), QStringLiteral("味精"),
                                  QStringLiteral("料酒"), QStringLiteral("香料")};
    return w;
}
const QStringList &kwDrink()
{
    static const QStringList w = {QStringLiteral("饮料"), QStringLiteral("可乐"), QStringLiteral("果汁"),
                                  QStringLiteral("茶"), QStringLiteral("咖啡")};
    return w;
}

QString mapUsdaLabel(const QString &label)
{
    const QString l = label.toLower();
    if (l.contains(QLatin1String("vegetab"))) return QStringLiteral("蔬菜类及制品");
    if (l.contains(QLatin1String("fruit"))) return QStringLiteral("水果类及制品");
    if (l.contains(QLatin1String("pork")) || l.contains(QLatin1String("beef"))
        || l.contains(QLatin1String("lamb")) || l.contains(QLatin1String("meat")))
        return QStringLiteral("畜肉类及制品");
    if (l.contains(QLatin1String("poultry")) || l.contains(QLatin1String("chicken"))
        || l.contains(QLatin1String("turkey")))
        return QStringLiteral("禽肉类及制品");
    if (l.contains(QLatin1String("dairy"))) return QStringLiteral("乳类及制品");
    if (l.contains(QLatin1String("egg"))) return QStringLiteral("蛋类及制品");
    if (l.contains(QLatin1String("finfish")) || l.contains(QLatin1String("shellfish"))
        || l.contains(QLatin1String("fish")))
        return QStringLiteral("鱼虾蟹贝类");
    if (l.contains(QLatin1String("cereal")) || l.contains(QLatin1String("pasta"))
        || l.contains(QLatin1String("baked")) || l.contains(QLatin1String("grain")))
        return QStringLiteral("谷类及制品");
    if (l.contains(QLatin1String("legume"))) return QStringLiteral("干豆类及制品");
    if (l.contains(QLatin1String("nut")) || l.contains(QLatin1String("seed")))
        return QStringLiteral("坚果、种子类");
    if (l.contains(QLatin1String("fat")) || l.contains(QLatin1String("oil"))
        || l.contains(QLatin1String("spice")))
        return QStringLiteral("油脂及调味品");
    if (l.contains(QLatin1String("beverage"))) return QStringLiteral("饮料类");
    return {};
}
} // namespace

FoodSearchWidget::FoodSearchWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(34, 28, 34, 26);
    root->setSpacing(14);

    auto *titleRow = new QHBoxLayout;
    auto *titleBox = new QVBoxLayout;
    titleBox->setSpacing(5);
    auto *eyebrow = new QLabel(QStringLiteral("NUTRITION FOOD GROUPS"), this);
    eyebrow->setObjectName(QStringLiteral("FoodEyebrow"));
    auto *title = new QLabel(QStringLiteral("食材分好类，选择更有根据。"), this);
    title->setObjectName(QStringLiteral("FoodPageTitle"));
    auto *hint = new QLabel(QStringLiteral("参照食物分类方式整理。点击分类卡片即可筛选下方营养数据。"), this);
    hint->setProperty("class", QVariant(QStringLiteral("HintText")));
    titleBox->addWidget(eyebrow);
    titleBox->addWidget(title);
    titleBox->addWidget(hint);

    m_countLabel = new QLabel(this);
    m_countLabel->setObjectName(QStringLiteral("FoodCountLabel"));
    m_countLabel->setAlignment(Qt::AlignCenter);
    m_countLabel->setMinimumWidth(110);
    titleRow->addLayout(titleBox);
    titleRow->addStretch();
    titleRow->addWidget(m_countLabel, 0, Qt::AlignBottom);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName(QStringLiteral("FoodSearchInput"));
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索食材、食物分类或子分类，例如：鸡、谷物、叶菜…"));
    m_searchEdit->setClearButtonEnabled(true);

    auto *classificationLabel = new QLabel(QStringLiteral("食物分类  /  FOOD GROUPS"), this);
    classificationLabel->setObjectName(QStringLiteral("FoodCategorySectionTitle"));

    auto *categoryPanel = new QFrame(this);
    categoryPanel->setObjectName(QStringLiteral("FoodCategoryPanel"));
    auto *categoryGrid = new QGridLayout(categoryPanel);
    categoryGrid->setContentsMargins(13, 13, 13, 13);
    categoryGrid->setHorizontalSpacing(10);
    categoryGrid->setVerticalSpacing(10);

    for (int i = 0; i < static_cast<int>(sizeof(kCategories) / sizeof(kCategories[0])); ++i) {
        const auto &definition = kCategories[i];
        auto *button = new QPushButton(
            QStringLiteral("%1\n%2")
                .arg(QString::fromUtf8(definition.name), QString::fromUtf8(definition.detail)),
            categoryPanel);
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setProperty("class", QVariant(QStringLiteral("CategoryFilter")));
        button->setProperty("category", QString::fromUtf8(definition.name));
        button->setMinimumHeight(54);
        button->setToolTip(QString::fromUtf8(definition.detail));
        m_categoryButtons.append(button);
        categoryGrid->addWidget(button, i / 3, i % 3);
        connect(button, &QPushButton::clicked, this, &FoodSearchWidget::onCategoryClicked);
    }
    if (!m_categoryButtons.isEmpty())
        m_categoryButtons.first()->setChecked(true);

    auto *tableTitleRow = new QHBoxLayout;
    auto *tableTitle = new QLabel(QStringLiteral("食材营养数据"), this);
    tableTitle->setProperty("class", QVariant(QStringLiteral("SectionTitle")));
    auto *tableHint = new QLabel(QStringLiteral("每 100g 可食部分 · 单次最多显示 200 条"), this);
    tableHint->setProperty("class", QVariant(QStringLiteral("HintText")));
    tableTitleRow->addWidget(tableTitle);
    tableTitleRow->addStretch();
    tableTitleRow->addWidget(tableHint);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({QStringLiteral("食材名称"), QStringLiteral("食物大类"),
                                        QStringLiteral("子分类"), QStringLiteral("热量(kcal)"),
                                        QStringLiteral("蛋白质(g)"), QStringLiteral("脂肪(g)"),
                                        QStringLiteral("碳水(g)")});
    // ResizeToContents 会在每次筛选时扫描所有单元格，食材数据多时容易让界面失去响应。
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 7; ++column)
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Interactive);
    m_table->setColumnWidth(1, 128);
    m_table->setColumnWidth(2, 132);
    for (int column = 3; column < 7; ++column)
        m_table->setColumnWidth(column, 88);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setWordWrap(false);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &FoodSearchWidget::onSearchTextChanged);

    root->addLayout(titleRow);
    root->addWidget(m_searchEdit);
    root->addWidget(classificationLabel);
    root->addWidget(categoryPanel);
    root->addLayout(tableTitleRow);
    root->addWidget(m_table, 1);
}

void FoodSearchWidget::reload()
{
    if (!DatabaseManager::getInstance().isOpen()) {
        m_rows.clear();
        m_loaded = false;
        updateCategoryButtons();
        refreshResults();
        return;
    }

    // 已加载则只刷新筛选，避免每次切入食材库都全表重扫卡顿
    if (m_loaded && !m_rows.isEmpty()) {
        updateCategoryButtons();
        refreshResults();
        return;
    }

    FoodDAO dao;
    const QList<Food> foods = dao.findAll(3000);
    m_rows.clear();
    m_rows.reserve(foods.size());
    for (const Food &food : foods) {
        FoodRow row;
        row.food = food;
        row.category = categoryForFood(food);
        row.subcategory = subcategoryForCategory(row.category);
        m_rows.append(row);
    }
    m_loaded = true;
    updateCategoryButtons();
    refreshResults();
}

void FoodSearchWidget::onSearchTextChanged(const QString &)
{
    refreshResults();
}

void FoodSearchWidget::onCategoryClicked()
{
    auto *clicked = qobject_cast<QPushButton *>(sender());
    if (!clicked)
        return;
    m_selectedCategory = clicked->property("category").toString();
    for (QPushButton *button : m_categoryButtons) {
        const QSignalBlocker blocker(button);
        button->setChecked(button == clicked);
    }
    refreshResults();
}

void FoodSearchWidget::refreshResults()
{
    if (m_refreshing)
        return;
    m_refreshing = true;
    if (!m_loaded && m_rows.isEmpty() && DatabaseManager::getInstance().isOpen())
        reload();

    const QString keyword = m_searchEdit ? m_searchEdit->text().trimmed() : QString();
    QList<const FoodRow *> filtered;
    filtered.reserve(m_rows.size());
    for (const FoodRow &row : m_rows) {
        const bool matchesCategory =
            m_selectedCategory.contains(QStringLiteral("全部"))
            || m_selectedCategory == QLatin1String("全部分类")
            || row.category == m_selectedCategory;
        if (!matchesCategory)
            continue;
        if (!keyword.isEmpty()) {
            const bool matchesSearch = row.food.name.contains(keyword, Qt::CaseInsensitive)
                || row.category.contains(keyword, Qt::CaseInsensitive)
                || row.subcategory.contains(keyword, Qt::CaseInsensitive);
            if (!matchesSearch)
                continue;
        }
        filtered.append(&row);
    }

    constexpr int kPreviewLimit = 120;
    populateTable(filtered.mid(0, kPreviewLimit));
    m_countLabel->setText(QStringLiteral("%1\n%2 条食材")
                              .arg(m_selectedCategory)
                              .arg(filtered.size()));
    m_refreshing = false;
}

QString FoodSearchWidget::categoryForFood(const Food &food) const
{
    if (!food.category.isEmpty()) {
        // 已是中文大类
        for (const auto &def : kCategories) {
            if (food.category == QLatin1String(def.name))
                return food.category;
        }
        const QString mapped = mapUsdaLabel(food.category);
        if (!mapped.isEmpty())
            return mapped;
    }

    const QString &name = food.name;
    if (containsAny(name, kwFungi())) return QStringLiteral("菌藻类");
    if (containsAny(name, kwBean())) return QStringLiteral("干豆类及制品");
    if (containsAny(name, kwMilk())) return QStringLiteral("乳类及制品");
    if (containsAny(name, kwPoultry())) return QStringLiteral("禽肉类及制品");
    if (containsAny(name, kwMeat())) return QStringLiteral("畜肉类及制品");
    if (containsAny(name, kwEgg())) return QStringLiteral("蛋类及制品");
    if (containsAny(name, kwSeafood())) return QStringLiteral("鱼虾蟹贝类");
    if (containsAny(name, kwGrain())) return QStringLiteral("谷类及制品");
    if (containsAny(name, kwStarch())) return QStringLiteral("薯类、淀粉及制品");
    if (containsAny(name, kwFruit())) return QStringLiteral("水果类及制品");
    if (containsAny(name, kwNut())) return QStringLiteral("坚果、种子类");
    if (containsAny(name, kwSeasoning())) return QStringLiteral("油脂及调味品");
    if (containsAny(name, kwDrink())) return QStringLiteral("饮料类");
    return QStringLiteral("蔬菜类及制品");
}

QString FoodSearchWidget::subcategoryForCategory(const QString &category) const
{
    if (category == QLatin1String("蔬菜类及制品")) return QStringLiteral("叶菜、根茎或瓜果类");
    if (category == QLatin1String("谷类及制品")) return QStringLiteral("米面或杂粮");
    if (category == QLatin1String("鱼虾蟹贝类")) return QStringLiteral("鱼、虾、蟹或贝类");
    if (category == QLatin1String("油脂及调味品")) return QStringLiteral("油脂或调味品");
    return category;
}

void FoodSearchWidget::updateCategoryButtons()
{
    QHash<QString, int> counts;
    counts.reserve(16);
    for (const FoodRow &row : m_rows) {
        if (!row.category.isEmpty())
            counts[row.category] += 1;
    }
    const int total = m_rows.size();

    for (int i = 0; i < m_categoryButtons.size(); ++i) {
        QPushButton *button = m_categoryButtons.at(i);
        const QString category = button->property("category").toString();
        // 第一项或名称匹配均视为「全部」
        const bool isAll = (i == 0) || category.contains(QStringLiteral("全部"));
        const int count = isAll ? total : counts.value(category, 0);
        const QString detail = button->toolTip();
        const QSignalBlocker blocker(button);
        button->setText(QStringLiteral("%1  ·  %2\n%3").arg(category).arg(count).arg(detail));
        button->setChecked(isAll ? (m_selectedCategory.contains(QStringLiteral("全部"))
                                    || m_selectedCategory == category)
                                 : (category == m_selectedCategory));
    }
}

void FoodSearchWidget::populateTable(const QList<const FoodRow *> &rows)
{
    const QSignalBlocker blocker(m_table);
    m_table->setUpdatesEnabled(false);
    m_table->setSortingEnabled(false);
    m_table->clearContents();
    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const FoodRow *row = rows.at(i);
        m_table->setItem(i, 0, new QTableWidgetItem(row->food.name));
        m_table->setItem(i, 1, new QTableWidgetItem(row->category));
        m_table->setItem(i, 2, new QTableWidgetItem(row->subcategory));
        m_table->setItem(i, 3, new QTableWidgetItem(QString::number(row->food.calories, 'f', 1)));
        m_table->setItem(i, 4, new QTableWidgetItem(QString::number(row->food.protein, 'f', 1)));
        m_table->setItem(i, 5, new QTableWidgetItem(QString::number(row->food.fat, 'f', 1)));
        m_table->setItem(i, 6, new QTableWidgetItem(QString::number(row->food.carbs, 'f', 1)));
    }
    m_table->setUpdatesEnabled(true);
    m_table->viewport()->update();
}
