#include "FoodSearchWidget.h"
#include "FoodDetailDialog.h"
#include "IngredientVisionDialog.h"
#include "UiAssets.h"

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
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {
struct CategoryDefinition {
    const char *name;
    const char *icon;
    const char *color;
};

const CategoryDefinition kCategories[] = {
    {"全部", "category-grid", "#059669"}, {"谷物", "grain", "#059669"},
    {"菌藻类", "mushroom", "#E64C4C"}, {"豆制品", "beans", "#F28A19"},
    {"蔬菜", "leaf", "#28A745"}, {"水果", "apple", "#E84C55"},
    {"肉禽蛋", "meat", "#D94B4B"}, {"鱼虾海鲜", "fish", "#2D7DD2"},
    {"乳制品", "milk", "#2F80D0"}, {"坚果油脂", "nut", "#E07A21"},
    {"调味料", "condiment", "#E5484D"}, {"其他", "other", "#526079"},
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
    if (l.contains(QLatin1String("vegetab"))) return QStringLiteral("蔬菜");
    if (l.contains(QLatin1String("fruit"))) return QStringLiteral("水果");
    if (l.contains(QLatin1String("pork")) || l.contains(QLatin1String("beef"))
        || l.contains(QLatin1String("lamb")) || l.contains(QLatin1String("meat")))
        return QStringLiteral("肉禽蛋");
    if (l.contains(QLatin1String("poultry")) || l.contains(QLatin1String("chicken"))
        || l.contains(QLatin1String("turkey")))
        return QStringLiteral("肉禽蛋");
    if (l.contains(QLatin1String("egg"))) return QStringLiteral("肉禽蛋");
    if (l.contains(QLatin1String("dairy"))) return QStringLiteral("乳制品");
    if (l.contains(QLatin1String("finfish")) || l.contains(QLatin1String("shellfish"))
        || l.contains(QLatin1String("fish")))
        return QStringLiteral("鱼虾海鲜");
    if (l.contains(QLatin1String("cereal")) || l.contains(QLatin1String("pasta"))
        || l.contains(QLatin1String("baked")) || l.contains(QLatin1String("grain")))
        return QStringLiteral("谷物");
    if (l.contains(QLatin1String("legume"))) return QStringLiteral("豆制品");
    if (l.contains(QLatin1String("nut")) || l.contains(QLatin1String("seed")))
        return QStringLiteral("坚果油脂");
    if (l.contains(QLatin1String("fat")) || l.contains(QLatin1String("oil"))
        || l.contains(QLatin1String("spice")))
        return QStringLiteral("坚果油脂");
    if (l.contains(QLatin1String("beverage"))) return QStringLiteral("其他");
    return {};
}

QFrame *makeFoodSummary(QWidget *parent, const QString &title, const QString &iconName,
                        const QString &tone, QLabel **value)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("FoodSummaryCard"));
    card->setProperty("tone", tone);
    auto *layout = new QHBoxLayout(card);
    layout->setContentsMargins(18, 12, 18, 12);
    layout->setSpacing(14);
    auto *icon = new QLabel(card);
    icon->setObjectName(QStringLiteral("FoodSummaryIcon"));
    icon->setProperty("tone", tone);
    icon->setAlignment(Qt::AlignCenter);
    icon->setFixedSize(46, 46);
    const QColor color(tone == QStringLiteral("green") ? QStringLiteral("#08A96E")
                       : tone == QStringLiteral("orange") ? QStringLiteral("#E99A2E")
                                                            : QStringLiteral("#725DD4"));
    icon->setPixmap(UiAssets::svgPixmap(iconName, QSize(24, 24), color, icon));
    auto *copy = new QVBoxLayout;
    copy->setSpacing(1);
    auto *caption = new QLabel(title, card);
    caption->setObjectName(QStringLiteral("FoodSummaryCaption"));
    *value = new QLabel(QStringLiteral("0"), card);
    (*value)->setObjectName(QStringLiteral("FoodSummaryValue"));
    (*value)->setProperty("tone", tone);
    copy->addWidget(caption);
    copy->addWidget(*value);
    layout->addWidget(icon);
    layout->addLayout(copy, 1);
    return card;
}
} // namespace

FoodSearchWidget::FoodSearchWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(23, 12, 30, 18);
    root->setSpacing(15);

    auto *titleRow = new QHBoxLayout;
    auto *titleBox = new QVBoxLayout;
    titleBox->setSpacing(5);
    auto *title = new QLabel(QStringLiteral("食材营养库"), this);
    title->setObjectName(QStringLiteral("FoodPageTitle"));
    auto *hint = new QLabel(QStringLiteral("按名称或分类查询食材的每 100g 营养数据"), this);
    hint->setProperty("class", QVariant(QStringLiteral("HintText")));
    titleBox->addWidget(title);
    titleBox->addWidget(hint);

    m_countLabel = new QLabel(this);
    m_countLabel->setObjectName(QStringLiteral("FoodSummaryValue"));
    m_countLabel->setAlignment(Qt::AlignCenter);
    m_countLabel->setMinimumWidth(110);
    m_countLabel->hide();
    titleRow->addLayout(titleBox);
    titleRow->addStretch();
    titleRow->addWidget(m_countLabel, 0, Qt::AlignBottom);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName(QStringLiteral("FoodSearchInput"));
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索食材名称"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedWidth(274);
    m_searchEdit->setFixedHeight(38);
    m_searchEdit->addAction(UiAssets::svgIcon(QStringLiteral("search")),
                            QLineEdit::LeadingPosition);
    m_selectedCategory = QStringLiteral("全部");

    auto *classificationLabel = new QLabel(QStringLiteral("分类筛选"), this);
    classificationLabel->setObjectName(QStringLiteral("FoodCategorySectionTitle"));

    auto *categoryPanel = new QFrame(this);
    categoryPanel->setObjectName(QStringLiteral("FoodCategoryPanel"));
    auto *categoryBox = new QVBoxLayout(categoryPanel);
    categoryBox->setContentsMargins(14, 14, 14, 14);
    categoryBox->setSpacing(8);
    auto *firstCategoryRow = new QHBoxLayout;
    auto *secondCategoryRow = new QHBoxLayout;
    firstCategoryRow->setSpacing(10);
    secondCategoryRow->setSpacing(10);

    for (int i = 0; i < static_cast<int>(sizeof(kCategories) / sizeof(kCategories[0])); ++i) {
        const auto &definition = kCategories[i];
        auto *button = new QPushButton(QString::fromUtf8(definition.name), categoryPanel);
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setProperty("class", QVariant(QStringLiteral("CategoryFilter")));
        button->setProperty("category", QString::fromUtf8(definition.name));
        button->setFixedHeight(34);
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        const int categoryWidth = qBound(
            61, button->fontMetrics().horizontalAdvance(button->text()) + 40, 104);
        button->setFixedWidth(categoryWidth);
        UiAssets::setButtonIcon(button, QString::fromUtf8(definition.icon), 18,
                                QColor(QString::fromLatin1(definition.color)));
        m_categoryButtons.append(button);
        if (i < 8)
            firstCategoryRow->addWidget(button);
        else
            secondCategoryRow->addWidget(button);
        connect(button, &QPushButton::clicked, this, &FoodSearchWidget::onCategoryClicked);
    }
    firstCategoryRow->addStretch();
    secondCategoryRow->addStretch();
    categoryBox->addLayout(firstCategoryRow);
    categoryBox->addLayout(secondCategoryRow);
    if (!m_categoryButtons.isEmpty())
        m_categoryButtons.first()->setChecked(true);

    auto *tableTitleRow = new QHBoxLayout;
    auto *tableTitle = new QLabel(QStringLiteral("食材营养数据"), this);
    tableTitle->setProperty("class", QVariant(QStringLiteral("SectionTitle")));
    auto *tableHint = new QLabel(QStringLiteral("以下均为每 100g 可食部分营养值"), this);
    tableHint->setProperty("class", QVariant(QStringLiteral("HintText")));
    tableTitleRow->addWidget(tableTitle);
    tableTitleRow->addStretch();
    tableTitleRow->addWidget(tableHint);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({QStringLiteral("食材名称"), QStringLiteral("分类"),
                                        QStringLiteral("热量(kcal)"),
                                        QStringLiteral("蛋白质(g)"), QStringLiteral("脂肪(g)"),
                                        QStringLiteral("碳水(g)"), QStringLiteral("收藏")});
    // ResizeToContents 会在每次筛选时扫描所有单元格，食材数据多时容易让界面失去响应。
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 7; ++column)
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Interactive);
    m_table->setColumnWidth(1, 105);
    for (int column = 2; column < 6; ++column)
        m_table->setColumnWidth(column, 92);
    m_table->setColumnWidth(6, 60);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(48);
    m_table->horizontalHeader()->setFixedHeight(52);
    m_table->setShowGrid(false);
    m_table->setWordWrap(false);
    m_table->setToolTip(QStringLiteral("点击最右侧星标收藏食材"));

    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(160);
    connect(m_searchTimer, &QTimer::timeout, this, [this]() { refreshResults(); });
    connect(m_searchEdit, &QLineEdit::textChanged, this, &FoodSearchWidget::onSearchTextChanged);
    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int column) {
        if (column == 6 || row < 0)
            return;
        const QTableWidgetItem *item = m_table->item(row, 0);
        const FoodRow *foodRow = item ? foodRowById(item->data(Qt::UserRole).toInt()) : nullptr;
        if (foodRow)
            openFoodDetail(*foodRow);
    });

    title->hide();
    hint->hide();
    classificationLabel->hide();
    tableTitle->hide();
    tableHint->hide();

    auto *summaryRow = new QWidget(this);
    summaryRow->setObjectName(QStringLiteral("FoodSummaryRow"));
    summaryRow->setFixedHeight(87);
    auto *summaryLay = new QHBoxLayout(summaryRow);
    summaryLay->setContentsMargins(0, 0, 0, 0);
    summaryLay->setSpacing(14);
    QWidget *recordedSummary = makeFoodSummary(summaryRow, QStringLiteral("已收录"),
                                               QStringLiteral("database"), QStringLiteral("green"),
                                               &m_countLabel);
    recordedSummary->setFixedWidth(275);
    summaryLay->addWidget(recordedSummary);
    summaryLay->addWidget(makeFoodSummary(summaryRow, QStringLiteral("常用分类"),
                                          QStringLiteral("category-grid"), QStringLiteral("orange"),
                                          &m_categoryCount), 1);
    summaryLay->addWidget(makeFoodSummary(summaryRow, QStringLiteral("我的收藏"),
                                          QStringLiteral("star-outline"), QStringLiteral("purple"),
                                          &m_favoriteCount), 1);
    categoryPanel->setFixedHeight(115);
    root->addWidget(summaryRow);
    root->addWidget(categoryPanel);
    auto *searchActions = new QHBoxLayout;
    searchActions->setSpacing(12);
    auto *visionButton = new QPushButton(QStringLiteral("拍照识别食材"), this);
    visionButton->setObjectName(QStringLiteral("FoodVisionEntryButton"));
    visionButton->setFixedSize(154, 40);
    visionButton->setToolTip(QStringLiteral("识别食材名称、特点、常见用途和可用菜谱"));
    UiAssets::setButtonIcon(visionButton, QStringLiteral("camera"), 18,
                            QColor(QStringLiteral("#08A96E")));
    searchActions->addWidget(m_searchEdit);
    searchActions->addStretch();
    searchActions->addWidget(visionButton);
    root->addLayout(searchActions);
    root->addWidget(m_table, 1);

    connect(visionButton, &QPushButton::clicked, this,
            &FoodSearchWidget::openIngredientVision);
}

void FoodSearchWidget::setUserId(int userId)
{
    if (m_userId == userId)
        return;
    m_userId = userId;
    if (m_loaded)
        refreshResults();
}

const FoodRow *FoodSearchWidget::foodRowById(int foodId) const
{
    for (const FoodRow &row : m_rows) {
        if (row.food.id == foodId)
            return &row;
    }
    return nullptr;
}

void FoodSearchWidget::openFoodDetail(const FoodRow &row, int reviewFavoriteState)
{
    QWidget *owner = window();
    FoodDetailDialog dialog(row.food, row.category, m_userId, owner, reviewFavoriteState);
    connect(&dialog, &FoodDetailDialog::favoriteChanged, this, [this](int, bool) {
        refreshResults();
        emit foodFavoriteChanged();
    });
    dialog.exec();
}

void FoodSearchWidget::openReviewDetail(bool favoriteState)
{
    if (!m_loaded)
        reload();
    const FoodRow *reviewRow = nullptr;
    for (const FoodRow &row : m_rows) {
        if (row.food.name == QStringLiteral("燕麦片")) {
            reviewRow = &row;
            break;
        }
    }
    if (!reviewRow && !m_rows.isEmpty())
        reviewRow = &m_rows.first();
    if (reviewRow)
        openFoodDetail(*reviewRow, favoriteState ? 1 : 0);
}

void FoodSearchWidget::setUsdaReviewState()
{
    if (!m_loaded)
        reload();
    m_selectedCategory = QStringLiteral("全部");
    if (m_searchEdit)
        m_searchEdit->clear();
    updateCategoryButtons();
    refreshResults();
}

void FoodSearchWidget::openIngredientVision()
{
    IngredientVisionDialog dialog(m_userId, window());
    dialog.exec();
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
    if (m_searchTimer)
        m_searchTimer->start();
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
        const bool matchesCategory = m_selectedCategory == QStringLiteral("全部")
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
    m_countLabel->setText(QStringLiteral("%1 种").arg(m_rows.size()));
    if (m_categoryCount)
        m_categoryCount->setText(QStringLiteral("12"));
    if (m_favoriteCount)
        m_favoriteCount->setText(QString::number(m_userId > 0
                                                     ? FoodDAO().findFavorites(m_userId).size()
                                                     : 0));
    m_refreshing = false;
}

QString FoodSearchWidget::categoryForFood(const Food &food) const
{
    if (!food.category.isEmpty()) {
        const QString c = food.category;
        if (c.contains(QStringLiteral("菌")) || c.contains(QStringLiteral("藻")))
            return QStringLiteral("菌藻类");
        if (c.contains(QStringLiteral("豆")))
            return QStringLiteral("豆制品");
        if (c.contains(QStringLiteral("蔬菜")))
            return QStringLiteral("蔬菜");
        if (c.contains(QStringLiteral("水果")))
            return QStringLiteral("水果");
        if (c.contains(QStringLiteral("畜肉")) || c.contains(QStringLiteral("禽肉"))
            || c.contains(QStringLiteral("蛋类")))
            return QStringLiteral("肉禽蛋");
        if (c.contains(QStringLiteral("鱼")) || c.contains(QStringLiteral("虾"))
            || c.contains(QStringLiteral("蟹")) || c.contains(QStringLiteral("贝")))
            return QStringLiteral("鱼虾海鲜");
        if (c.contains(QStringLiteral("乳")))
            return QStringLiteral("乳制品");
        if (c.contains(QStringLiteral("谷")) || c.contains(QStringLiteral("薯"))
            || c.contains(QStringLiteral("淀粉")))
            return QStringLiteral("谷物");
        if (c.contains(QStringLiteral("坚果")) || c.contains(QStringLiteral("种子")))
            return QStringLiteral("坚果油脂");
        if (c.contains(QStringLiteral("调味")))
            return QStringLiteral("调味料");
        const QString mapped = mapUsdaLabel(food.category);
        if (!mapped.isEmpty())
            return mapped;
    }

    const QString &name = food.name;
    if (containsAny(name, kwFungi())) return QStringLiteral("菌藻类");
    if (containsAny(name, kwBean())) return QStringLiteral("豆制品");
    if (containsAny(name, kwMilk())) return QStringLiteral("乳制品");
    if (containsAny(name, kwPoultry()) || containsAny(name, kwMeat())
        || containsAny(name, kwEgg())) return QStringLiteral("肉禽蛋");
    if (containsAny(name, kwSeafood())) return QStringLiteral("鱼虾海鲜");
    if (containsAny(name, kwGrain()) || containsAny(name, kwStarch()))
        return QStringLiteral("谷物");
    if (containsAny(name, kwFruit())) return QStringLiteral("水果");
    if (containsAny(name, kwSeasoning())) return QStringLiteral("调味料");
    if (containsAny(name, kwNut())) return QStringLiteral("坚果油脂");
    if (containsAny(name, kwDrink())) return QStringLiteral("其他");
    return QStringLiteral("其他");
}

QString FoodSearchWidget::subcategoryForCategory(const QString &category) const
{
    if (category == QStringLiteral("蔬菜类及制品")) return QStringLiteral("叶菜、根茎或瓜果类");
    if (category == QStringLiteral("谷类及制品")) return QStringLiteral("米面或杂粮");
    if (category == QStringLiteral("鱼虾蟹贝类")) return QStringLiteral("鱼、虾、蟹或贝类");
    if (category == QStringLiteral("油脂及调味品")) return QStringLiteral("油脂或调味品");
    return category;
}

void FoodSearchWidget::updateCategoryButtons()
{
    for (int i = 0; i < m_categoryButtons.size(); ++i) {
        QPushButton *button = m_categoryButtons.at(i);
        const QString category = button->property("category").toString();
        const bool isAll = (i == 0) || category == QStringLiteral("全部");
        const QSignalBlocker blocker(button);
        button->setText(category);
        button->setChecked(isAll ? (m_selectedCategory == QStringLiteral("全部")
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
        auto *nameItem = new QTableWidgetItem(row->food.name);
        nameItem->setData(Qt::UserRole, row->food.id);
        m_table->setItem(i, 0, nameItem);
        m_table->setItem(i, 1, new QTableWidgetItem(row->category));
        m_table->setItem(i, 2, new QTableWidgetItem(QString::number(row->food.calories, 'f', 1)));
        m_table->setItem(i, 3, new QTableWidgetItem(QString::number(row->food.protein, 'f', 1)));
        m_table->setItem(i, 4, new QTableWidgetItem(QString::number(row->food.fat, 'f', 1)));
        m_table->setItem(i, 5, new QTableWidgetItem(QString::number(row->food.carbs, 'f', 1)));
        auto *favorite = new QToolButton(m_table);
        favorite->setObjectName(QStringLiteral("FoodFavoriteStar"));
        favorite->setCursor(Qt::PointingHandCursor);
        favorite->setAutoRaise(true);
        favorite->setToolTip(QStringLiteral("收藏/取消收藏 %1").arg(row->food.name));
        const bool checked = m_userId > 0 && FoodDAO().isFavorite(m_userId, row->food.id);
        favorite->setChecked(checked);
        favorite->setCheckable(true);
        auto updateStar = [favorite](bool selected) {
            UiAssets::setButtonIcon(favorite,
                                    selected ? QStringLiteral("star-filled")
                                             : QStringLiteral("star-outline"),
                                    22, selected ? QColor(QStringLiteral("#F2A23A"))
                                                : QColor(QStringLiteral("#7A859A")));
        };
        updateStar(checked);
        const int foodId = row->food.id;
        connect(favorite, &QToolButton::clicked, this,
                [this, favorite, foodId, updateStar](bool desired) {
            if (m_userId <= 0) {
                favorite->setChecked(false);
                updateStar(false);
                return;
            }
            FoodDAO dao;
            if (!dao.setFavorite(m_userId, foodId, desired)) {
                favorite->setChecked(!desired);
                updateStar(!desired);
                QMessageBox::warning(this, QStringLiteral("收藏失败"),
                                     QStringLiteral("收藏状态未保存，请稍后重试。"));
                return;
            }
            updateStar(desired);
            if (m_favoriteCount)
                m_favoriteCount->setText(QString::number(FoodDAO().findFavorites(m_userId).size()));
            emit foodFavoriteChanged();
        });
        m_table->setCellWidget(i, 6, favorite);
    }
    m_table->setUpdatesEnabled(true);
    m_table->viewport()->update();
}
