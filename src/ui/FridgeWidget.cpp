#include "FridgeWidget.h"
#include "ShoppingListDialog.h"
#include "FridgeVisionDialog.h"
#include "UiAssets.h"

#include "../dao/FridgeDAO.h"
#include "../dao/RecipeDAO.h"
#include "../engine/FridgeClearEngine.h"
#include "../entities/FridgeItem.h"
#include "../services/RecipeImageProvider.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDate>
#include <QDateEdit>
#include <QFrame>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSizePolicy>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
QString formatQty(double qty)
{
    return qFuzzyCompare(qty, qRound(qty)) ? QString::number(int(qRound(qty)))
                                           : QString::number(qty, 'f', 1);
}

QString foodCategory(const QString &name)
{
    if (name.contains(QStringLiteral("肉")) || name.contains(QStringLiteral("鸡"))
        || name.contains(QStringLiteral("鸭")) || name.contains(QStringLiteral("蛋")))
        return QStringLiteral("肉禽蛋");
    if (name.contains(QStringLiteral("鱼")) || name.contains(QStringLiteral("虾"))
        || name.contains(QStringLiteral("蟹")) || name.contains(QStringLiteral("贝")))
        return QStringLiteral("鱼虾海鲜");
    if (name.contains(QStringLiteral("米")) || name.contains(QStringLiteral("面"))
        || name.contains(QStringLiteral("麦")) || name.contains(QStringLiteral("粥")))
        return QStringLiteral("谷物");
    if (name.contains(QStringLiteral("奶")) || name.contains(QStringLiteral("乳")))
        return QStringLiteral("乳制品");
    if (name.contains(QStringLiteral("苹果")) || name.contains(QStringLiteral("梨"))
        || name.contains(QStringLiteral("橙")) || name.contains(QStringLiteral("果")))
        return QStringLiteral("水果");
    return QStringLiteral("蔬菜");
}

QFrame *makeSummaryCell(QWidget *parent, const QString &caption, const QString &unit,
                        QLabel **value, const QString &tone, const QString &iconName)
{
    auto *cell = new QFrame(parent);
    cell->setProperty("class", QStringLiteral("FridgeSummaryCell"));
    cell->setProperty("tone", tone);
    auto *lay = new QHBoxLayout(cell);
    lay->setContentsMargins(14, 7, 14, 7);
    lay->setSpacing(10);
    auto *icon = new QLabel(cell);
    icon->setObjectName(QStringLiteral("FridgeSummaryIcon"));
    icon->setProperty("tone", tone);
    icon->setAlignment(Qt::AlignCenter);
    icon->setFixedSize(38, 38);
    const QColor iconColor(tone == QStringLiteral("green") ? QStringLiteral("#08A96E")
                           : tone == QStringLiteral("blue") ? QStringLiteral("#567BDD")
                           : tone == QStringLiteral("orange") ? QStringLiteral("#E99A2E")
                                                                : QStringLiteral("#D75A52"));
    icon->setPixmap(UiAssets::svgPixmap(iconName, QSize(21, 21), iconColor, icon));
    auto *text = new QVBoxLayout;
    text->setSpacing(1);
    auto *label = new QLabel(caption, cell);
    label->setObjectName(QStringLiteral("FridgeSummaryCaption"));
    label->setFont(UiAssets::bodyFont(13, QFont::DemiBold));
    *value = new QLabel(QStringLiteral("0"), cell);
    (*value)->setObjectName(QStringLiteral("FridgeSummaryValue"));
    (*value)->setProperty("tone", tone);
    (*value)->setFont(UiAssets::bodyFont(26, QFont::ExtraBold));
    auto *unitLabel = new QLabel(unit, cell);
    unitLabel->setObjectName(QStringLiteral("FridgeSummaryUnit"));
    auto *valueRow = new QHBoxLayout;
    valueRow->setContentsMargins(0, 0, 0, 0);
    valueRow->setSpacing(5);
    valueRow->addWidget(*value, 0, Qt::AlignBottom);
    valueRow->addWidget(unitLabel, 0, Qt::AlignBottom);
    valueRow->addStretch();
    text->addWidget(label);
    text->addLayout(valueRow);
    lay->addWidget(icon);
    lay->addLayout(text, 1);
    return cell;
}

QWidget *makeStatusBadge(QWidget *parent, const QString &status)
{
    auto *host = new QWidget(parent);
    host->setObjectName(QStringLiteral("FridgeStatusHost"));
    auto *layout = new QHBoxLayout(host);
    layout->setContentsMargins(8, 7, 8, 7);
    auto *badge = new QLabel(status, host);
    badge->setObjectName(QStringLiteral("FridgeStatusBadge"));
    badge->setProperty("tone", status == QStringLiteral("已过期")
                                   ? QStringLiteral("red")
                                   : status == QStringLiteral("即将过期")
                                         ? QStringLiteral("orange")
                                         : QStringLiteral("green"));
    badge->setAlignment(Qt::AlignCenter);
    badge->setMinimumSize(76, 28);
    layout->addWidget(badge, 0, Qt::AlignCenter);
    return host;
}

QListWidgetItem *addRecipeResultItem(QListWidget *list, const Recipe &recipe,
                                     const QString &description)
{
    auto *item = new QListWidgetItem(list);
    item->setData(Qt::UserRole, recipe.id);
    item->setSizeHint(QSize(0, 72));

    auto *row = new QFrame(list);
    row->setObjectName(QStringLiteral("FridgeRecipeResult"));
    auto *lay = new QHBoxLayout(row);
    lay->setContentsMargins(8, 7, 10, 7);
    lay->setSpacing(12);

    auto *photo = new QLabel(row);
    photo->setObjectName(QStringLiteral("FridgeRecipeImage"));
    photo->setFixedSize(80, 54);
    photo->setPixmap(RecipeImageProvider::pixmap(recipe.name, photo->size()));

    auto *copy = new QVBoxLayout;
    copy->setSpacing(3);
    auto *name = new QLabel(recipe.name, row);
    name->setObjectName(QStringLiteral("FridgeRecipeName"));
    auto *meta = new QLabel(description, row);
    meta->setObjectName(QStringLiteral("FridgeRecipeMeta"));
    copy->addWidget(name);
    copy->addWidget(meta);

    auto *open = new QLabel(QStringLiteral("查看详情"), row);
    open->setObjectName(QStringLiteral("FridgeRecipeOpen"));
    open->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    open->setPixmap(UiAssets::svgPixmap(QStringLiteral("chevron-right"), QSize(18, 18),
                                        QColor(QStringLiteral("#28594A")), open));
    open->setToolTip(QStringLiteral("查看详情"));

    lay->addWidget(photo);
    lay->addLayout(copy, 1);
    lay->addWidget(open);
    list->setItemWidget(item, row);
    return item;
}
} // namespace

FridgeWidget::FridgeWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("FridgePage"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(17, 8, 35, 22);
    root->setSpacing(9);

    auto *toolbarPanel = new QFrame(this);
    toolbarPanel->setObjectName(QStringLiteral("FridgeToolbarPanel"));
    auto *toolbarStack = new QVBoxLayout(toolbarPanel);
    toolbarStack->setContentsMargins(0, 0, 0, 0);
    toolbarStack->setSpacing(7);
    auto *actionRow = new QHBoxLayout;
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(8);
    auto *recommendBtn = new QPushButton(QStringLiteral("生成清冰箱食谱"), this);
    recommendBtn->setProperty("class", QStringLiteral("PrimaryButton"));
    recommendBtn->setObjectName(QStringLiteral("FridgeRecipeButton"));
    recommendBtn->setFont(UiAssets::bodyFont(14, QFont::DemiBold));
    UiAssets::setButtonIcon(recommendBtn, QStringLiteral("recommend-star"), 18,
                            QColor(Qt::white));
    auto *addToggle = new QPushButton(QStringLiteral("添加食材"), this);
    addToggle->setObjectName(QStringLiteral("FridgeToolbarButton"));
    addToggle->setProperty("class", QStringLiteral("GhostButton"));
    UiAssets::setButtonIcon(addToggle, QStringLiteral("plus"), 18);
    auto *photoAdd = new QPushButton(QStringLiteral("拍照添加"), this);
    photoAdd->setObjectName(QStringLiteral("FridgePhotoAddButton"));
    photoAdd->setProperty("class", QStringLiteral("GhostButton"));
    UiAssets::setButtonIcon(photoAdd, QStringLiteral("camera"), 18,
                            QColor(QStringLiteral("#08A96E")));
    auto *shoppingBtn = new QPushButton(QStringLiteral("购物清单"), this);
    shoppingBtn->setObjectName(QStringLiteral("FridgeToolbarButton"));
    shoppingBtn->setProperty("class", QStringLiteral("GhostButton"));
    UiAssets::setButtonIcon(shoppingBtn, QStringLiteral("basket"), 18);
    auto configureAction = [](QPushButton *button) {
        button->setFixedSize(158, 46);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    };
    for (QPushButton *button : {recommendBtn, shoppingBtn, addToggle, photoAdd})
        configureAction(button);

    m_viewToggleBtn = new QPushButton(QStringLiteral("查看剩余食材"), this);
    m_viewToggleBtn->setObjectName(QStringLiteral("FridgeViewToggleButton"));
    m_viewToggleBtn->setProperty("class", QStringLiteral("GhostButton"));
    configureAction(m_viewToggleBtn);
    m_viewToggleBtn->hide();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName(QStringLiteral("FridgeSearchInput"));
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索食材"));
    m_searchEdit->addAction(UiAssets::svgIcon(QStringLiteral("search")),
                            QLineEdit::LeadingPosition);
    m_searchEdit->setMinimumWidth(230);
    m_searchEdit->setMaximumWidth(430);
    m_searchEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_statusFilter = new QComboBox(this);
    m_statusFilter->setObjectName(QStringLiteral("FridgeStatusFilter"));
    m_statusFilter->addItems({QStringLiteral("全部状态"), QStringLiteral("新鲜"),
                              QStringLiteral("即将过期"), QStringLiteral("已过期")});
    m_statusFilter->setFixedWidth(132);
    actionRow->addWidget(recommendBtn);
    actionRow->addWidget(shoppingBtn);
    actionRow->addWidget(addToggle);
    actionRow->addWidget(photoAdd);
    actionRow->addStretch();

    auto *filterRow = new QHBoxLayout;
    filterRow->setContentsMargins(0, 0, 0, 0);
    filterRow->setSpacing(8);
    filterRow->addWidget(m_viewToggleBtn);
    filterRow->addStretch(1);
    filterRow->addWidget(m_searchEdit, 2);
    filterRow->addWidget(m_statusFilter);
    toolbarStack->addLayout(actionRow);
    toolbarStack->addLayout(filterRow);
    root->addWidget(toolbarPanel);

    m_expiryReminder = new QFrame(this);
    m_expiryReminder->setObjectName(QStringLiteral("ExpiryReminder"));
    auto *reminderLayout = new QHBoxLayout(m_expiryReminder);
    reminderLayout->setContentsMargins(12, 8, 10, 8);
    reminderLayout->setSpacing(9);
    auto *reminderIcon = UiAssets::createIconLabel(
        m_expiryReminder, QStringLiteral("clock"), 19, QColor(QStringLiteral("#B96A3E")));
    m_expiryReminderText = new QLabel(m_expiryReminder);
    m_expiryReminderText->setWordWrap(true);
    auto *recommendExpiring = new QPushButton(QStringLiteral("用临期食材找菜"), m_expiryReminder);
    recommendExpiring->setObjectName(QStringLiteral("ExpiryRecipeButton"));
    recommendExpiring->setProperty("class", QStringLiteral("GhostButton"));
    UiAssets::setButtonIcon(recommendExpiring, QStringLiteral("recommend-star"), 17);
    reminderLayout->addWidget(reminderIcon);
    reminderLayout->addWidget(m_expiryReminderText, 1);
    reminderLayout->addWidget(recommendExpiring);
    root->addWidget(m_expiryReminder);
    m_expiryReminder->hide();

    m_addEditor = new QWidget(this);
    m_addEditor->setObjectName(QStringLiteral("FridgeAddEditor"));
    auto *addRow = new QHBoxLayout(m_addEditor);
    addRow->setContentsMargins(10, 8, 10, 8);
    addRow->setSpacing(6);
    m_addEdit = new QLineEdit(m_addEditor);
    m_addEdit->setPlaceholderText(QStringLiteral("食材名称，如：鸡蛋、鸡胸肉"));
    m_qtySpin = new QDoubleSpinBox(m_addEditor);
    m_qtySpin->setRange(0.1, 9999.0);
    m_qtySpin->setDecimals(1);
    m_qtySpin->setValue(100.0);
    m_qtySpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_qtySpin->setFixedWidth(88);
    m_unitCombo = new QComboBox(m_addEditor);
    m_unitCombo->addItems({QStringLiteral("g"), QStringLiteral("个"), QStringLiteral("份"),
                           QStringLiteral("斤"), QStringLiteral("ml")});
    m_unitCombo->setFixedWidth(66);
    m_expiryEdit = new QDateEdit(QDate::currentDate().addDays(7), m_addEditor);
    m_expiryEdit->setCalendarPopup(true);
    m_expiryEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_expiryEdit->setMinimumWidth(138);
    m_expiryEdit->setFixedWidth(138);
    auto *confirmAdd = new QPushButton(QStringLiteral("确认添加"), m_addEditor);
    confirmAdd->setProperty("class", QStringLiteral("PrimaryButton"));
    confirmAdd->setFixedSize(124, 44);
    auto *cancelAdd = new QPushButton(QStringLiteral("取消"), m_addEditor);
    cancelAdd->setProperty("class", QStringLiteral("GhostButton"));
    cancelAdd->setFixedSize(78, 44);
    addRow->addWidget(m_addEdit, 1);
    addRow->addWidget(m_qtySpin);
    addRow->addWidget(m_unitCombo);
    addRow->addWidget(m_expiryEdit);
    addRow->addWidget(confirmAdd);
    addRow->addWidget(cancelAdd);
    root->addWidget(m_addEditor);
    m_addEditor->hide();

    auto *summary = new QFrame(this);
    summary->setObjectName(QStringLiteral("FridgeSummary"));
    auto *summaryLay = new QHBoxLayout(summary);
    summaryLay->setContentsMargins(0, 0, 0, 0);
    summaryLay->setSpacing(0);
    summaryLay->addWidget(makeSummaryCell(summary, QStringLiteral("食材种类"), QStringLiteral("种"), &m_kindValue,
                                          QStringLiteral("green"), QStringLiteral("sprout")), 1);
    summaryLay->addWidget(makeSummaryCell(summary, QStringLiteral("食材总数"), QStringLiteral("份"), &m_totalValue,
                                          QStringLiteral("blue"), QStringLiteral("archive-box")), 1);
    summaryLay->addWidget(makeSummaryCell(summary, QStringLiteral("即将过期"), QStringLiteral("份"), &m_expiringValue,
                                          QStringLiteral("orange"), QStringLiteral("clock")), 1);
    summaryLay->addWidget(makeSummaryCell(summary, QStringLiteral("已过期"), QStringLiteral("份"), &m_expiredValue,
                                          QStringLiteral("red"), QStringLiteral("alert")), 1);
    summary->setFixedHeight(82);
    root->addWidget(summary);

    m_inventoryTable = new QTableWidget(this);
    m_inventoryTable->setObjectName(QStringLiteral("FridgeInventoryTable"));
    m_inventoryTable->setColumnCount(6);
    m_inventoryTable->setHorizontalHeaderLabels(
        {QStringLiteral("食材名称"), QStringLiteral("分类"), QStringLiteral("数量"),
         QStringLiteral("单位"), QStringLiteral("保质期"), QStringLiteral("状态")});
    m_inventoryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_inventoryTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_inventoryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_inventoryTable->setAlternatingRowColors(true);
    m_inventoryTable->setShowGrid(false);
    m_inventoryTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_inventoryTable->verticalHeader()->hide();
    m_inventoryTable->horizontalHeader()->setStretchLastSection(true);
    m_inventoryTable->horizontalHeader()->setFixedHeight(48);
    m_inventoryTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c < 6; ++c)
        m_inventoryTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    root->addWidget(m_inventoryTable, 1);

    auto *bottom = new QHBoxLayout;
    m_hintLabel = new QLabel(this);
    m_hintLabel->setProperty("class", QStringLiteral("HintText"));
    auto *removeBtn = new QPushButton(QStringLiteral("移除选中"), this);
    removeBtn->setProperty("class", QStringLiteral("LinkButton"));
    auto *clearBtn = new QPushButton(QStringLiteral("清空库存"), this);
    clearBtn->setProperty("class", QStringLiteral("LinkButton"));
    bottom->addWidget(m_hintLabel, 1);
    bottom->addWidget(removeBtn);
    bottom->addWidget(clearBtn);
    root->addLayout(bottom);

    m_resultPanel = new QFrame(this);
    m_resultPanel->setObjectName(QStringLiteral("FridgeResultPanel"));
    m_resultPanel->setMinimumHeight(320);
    m_resultPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *resultLay = new QVBoxLayout(m_resultPanel);
    resultLay->setContentsMargins(12, 10, 12, 10);
    m_resultHint = new QLabel(m_resultPanel);
    m_resultHint->setProperty("class", QStringLiteral("HintText"));
    m_resultList = new QListWidget(m_resultPanel);
    m_resultList->setObjectName(QStringLiteral("FridgeResultList"));
    m_resultList->setSpacing(4);
    m_resultList->setSelectionMode(QAbstractItemView::SingleSelection);
    resultLay->addWidget(m_resultHint);
    resultLay->addWidget(m_resultList, 1);
    root->addWidget(m_resultPanel, 1);
    m_resultPanel->hide();

    connect(addToggle, &QPushButton::clicked, this,
            [this]() { showAddEditor(!m_addEditor->isVisible()); });
    connect(photoAdd, &QPushButton::clicked, this, &FridgeWidget::onPhotoAdd);
    connect(cancelAdd, &QPushButton::clicked, this, [this]() { showAddEditor(false); });
    connect(confirmAdd, &QPushButton::clicked, this, &FridgeWidget::onAdd);
    connect(m_addEdit, &QLineEdit::returnPressed, this, &FridgeWidget::onAdd);
    connect(recommendBtn, &QPushButton::clicked, this, &FridgeWidget::onRecommend);
    connect(shoppingBtn, &QPushButton::clicked, this, &FridgeWidget::onShoppingList);
    connect(recommendExpiring, &QPushButton::clicked, this, &FridgeWidget::onRecommendExpiring);
    connect(m_viewToggleBtn, &QPushButton::clicked, this, &FridgeWidget::onToggleView);
    connect(removeBtn, &QPushButton::clicked, this, &FridgeWidget::onRemoveSelected);
    connect(clearBtn, &QPushButton::clicked, this, &FridgeWidget::onClearAll);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &FridgeWidget::onSearchChanged);
    connect(m_statusFilter, &QComboBox::currentTextChanged, this, &FridgeWidget::onSearchChanged);
    connect(m_inventoryTable, &QTableWidget::customContextMenuRequested, this,
            &FridgeWidget::onInventoryContextMenu);
    connect(m_inventoryTable, &QTableWidget::cellDoubleClicked, this,
            &FridgeWidget::onInventoryDoubleClicked);
    connect(m_resultList, &QListWidget::itemClicked, this, &FridgeWidget::onResultClicked);
}

void FridgeWidget::showAddEditor(bool visible)
{
    m_addEditor->setVisible(visible);
    if (visible)
        m_addEdit->setFocus();
}

void FridgeWidget::onPhotoAdd()
{
    if (m_user.id <= 0) {
        QMessageBox::information(this, QStringLiteral("请先登录"),
                                 QStringLiteral("登录后才能把识别结果加入冰箱。"));
        return;
    }
    FridgeVisionDialog *dialog = FridgeVisionDialog::create(m_user.id, this);
    connect(dialog, &FridgeVisionDialog::ingredientAdded,
            this, &FridgeWidget::refreshInventory);
    dialog->exec();
    delete dialog;
}

void FridgeWidget::setUser(const User &user) { m_user = user; }
void FridgeWidget::setPlan(const RecommendResult &plan) { m_plan = plan; }
void FridgeWidget::reload() { refreshInventory(); }

void FridgeWidget::refreshInventory()
{
    m_showingResults = false;
    m_resultPanel->hide();
    m_inventoryTable->show();
    m_viewToggleBtn->hide();
    m_inventoryTable->setRowCount(0);
    if (m_user.id <= 0) {
        m_hintLabel->setText(QStringLiteral("请先登录后再管理冰箱。"));
        return;
    }
    const QList<FridgeItem> items = FridgeDAO().listByUser(m_user.id);
    m_inventoryTable->setRowCount(items.size());
    int expiringCount = 0;
    int expiredCount = 0;
    QStringList expiringNames;
    const QDate today = QDate::currentDate();
    for (int row = 0; row < items.size(); ++row) {
        const FridgeItem &item = items.at(row);
        const QDate expiry = QDate::fromString(item.expiryDate, Qt::ISODate);
        QString status = QStringLiteral("新鲜");
        if (expiry.isValid() && expiry < today) {
            status = QStringLiteral("已过期");
            ++expiredCount;
        } else if (expiry.isValid() && today.daysTo(expiry) <= 3) {
            status = QStringLiteral("即将过期");
            ++expiringCount;
            expiringNames.append(item.foodName);
        }
        const QStringList values = {
            item.foodName, foodCategory(item.foodName), formatQty(item.quantity),
            item.unit.isEmpty() ? QStringLiteral("份") : item.unit,
            expiry.isValid() ? expiry.toString(QStringLiteral("yyyy-MM-dd")) : QStringLiteral("未设置"),
            status};
        for (int c = 0; c < values.size(); ++c) {
            auto *cell = new QTableWidgetItem(values.at(c));
            cell->setData(Qt::UserRole, item.id);
            cell->setData(Qt::UserRole + 1, item.foodName);
            cell->setData(Qt::UserRole + 2, item.quantity);
            cell->setData(Qt::UserRole + 3, item.unit);
            cell->setData(Qt::UserRole + 4, item.expiryDate);
            if (c == 5) {
                cell->setTextAlignment(Qt::AlignCenter);
                cell->setForeground(QColor(status == QStringLiteral("已过期")
                                               ? QStringLiteral("#D94045")
                                               : status == QStringLiteral("即将过期")
                                                     ? QStringLiteral("#F28A19")
                                                     : QStringLiteral("#08A96E")));
            }
            m_inventoryTable->setItem(row, c, cell);
            if (c == 5)
                m_inventoryTable->setCellWidget(row, c,
                                                makeStatusBadge(m_inventoryTable, status));
        }
        m_inventoryTable->setRowHeight(row, 52);
    }
    QSet<QString> categories;
    for (const FridgeItem &item : items)
        categories.insert(foodCategory(item.foodName));
    m_kindValue->setText(QString::number(categories.size()));
    m_totalValue->setText(QString::number(items.size()));
    m_expiringValue->setText(QString::number(expiringCount));
    m_expiredValue->setText(QString::number(expiredCount));
    if (m_expiryReminder) {
        const bool showReminder = expiringCount > 0 || expiredCount > 0;
        m_expiryReminder->setVisible(showReminder);
        if (showReminder) {
            QString message;
            if (expiringCount > 0)
                message = QStringLiteral("%1 种食材将在3天内过期：%2。")
                              .arg(expiringCount).arg(expiringNames.mid(0, 4).join(QStringLiteral("、")));
            if (expiredCount > 0)
                message += QStringLiteral("另有 %1 种已过期，请先检查后处理。").arg(expiredCount);
            m_expiryReminderText->setText(message);
        }
    }
    m_hintLabel->setText(items.isEmpty() ? QStringLiteral("库存为空，可点击“添加食材”开始记录。")
                                         : QStringLiteral("共记录 %1 种食材；双击条目可修改数量。")
                                               .arg(items.size()));
    onSearchChanged();
}

void FridgeWidget::onSearchChanged()
{
    const QString key = m_searchEdit->text().trimmed();
    const QString status = m_statusFilter->currentText();
    for (int row = 0; row < m_inventoryTable->rowCount(); ++row) {
        const bool keyOk = key.isEmpty()
                           || m_inventoryTable->item(row, 0)->text().contains(key, Qt::CaseInsensitive);
        const bool statusOk = status == QStringLiteral("全部状态")
                              || m_inventoryTable->item(row, 5)->text() == status;
        m_inventoryTable->setRowHidden(row, !(keyOk && statusOk));
    }
}

void FridgeWidget::onAdd()
{
    const QString name = m_addEdit->text().trimmed();
    if (m_user.id <= 0 || name.isEmpty())
        return;
    if (!FridgeDAO().upsert(m_user.id, name, m_qtySpin->value(), m_unitCombo->currentText(),
                            m_expiryEdit->date().toString(Qt::ISODate))) {
        QMessageBox::warning(this, QStringLiteral("失败"), QStringLiteral("写入冰箱失败。"));
        return;
    }
    m_addEdit->clear();
    m_expiryEdit->setDate(QDate::currentDate().addDays(7));
    showAddEditor(false);
    refreshInventory();
}

void FridgeWidget::onQuickAdd() {}

void FridgeWidget::onRemoveSelected()
{
    const int row = m_inventoryTable->currentRow();
    if (row < 0 || !m_inventoryTable->item(row, 0))
        return;
    FridgeDAO().removeById(m_inventoryTable->item(row, 0)->data(Qt::UserRole).toInt());
    refreshInventory();
}

void FridgeWidget::onInventoryDoubleClicked(int row, int)
{
    if (row < 0 || !m_inventoryTable->item(row, 0))
        return;
    auto *item = m_inventoryTable->item(row, 0);
    bool ok = false;
    const double qty = QInputDialog::getDouble(this, QStringLiteral("修改数量"), item->text(),
                                               item->data(Qt::UserRole + 2).toDouble(), 0.1,
                                               9999.0, 1, &ok);
    if (!ok)
        return;
    FridgeDAO().updateById(item->data(Qt::UserRole).toInt(), qty,
                           item->data(Qt::UserRole + 3).toString(),
                           item->data(Qt::UserRole + 4).toString());
    refreshInventory();
}

void FridgeWidget::onInventoryContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_inventoryTable->indexAt(pos);
    if (!index.isValid())
        return;
    m_inventoryTable->selectRow(index.row());
    QMenu menu(this);
    QAction *edit = menu.addAction(QStringLiteral("修改数量"));
    QAction *remove = menu.addAction(QStringLiteral("删除食材"));
    QAction *chosen = menu.exec(m_inventoryTable->viewport()->mapToGlobal(pos));
    if (chosen == edit)
        onInventoryDoubleClicked(index.row(), 2);
    else if (chosen == remove)
        onRemoveSelected();
}

void FridgeWidget::onClearAll()
{
    if (m_user.id <= 0)
        return;
    if (QMessageBox::question(this, QStringLiteral("清空冰箱"), QStringLiteral("确定清空全部库存？"))
        != QMessageBox::Yes)
        return;
    FridgeDAO().clearUser(m_user.id);
    refreshInventory();
}

void FridgeWidget::onRecommend()
{
    if (m_user.id <= 0)
        return;
    const QStringList foods = FridgeDAO().foodNames(m_user.id);
    if (foods.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先添加冰箱食材。"));
        return;
    }
    FridgeClearEngine engine(m_user);
    if (foods.size() >= 3) {
        const RecommendResult plan = engine.generateDailyPlan(foods);
        if (plan.valid) {
            showDailyPlan(plan);
            emit planGenerated(plan);
            return;
        }
    }
    showDishRecommendations(foods);
}

void FridgeWidget::onShoppingList()
{
    if (m_user.id <= 0)
        return;
    if (!m_plan.valid) {
        QMessageBox::information(this, QStringLiteral("暂无方案"),
                                 QStringLiteral("请先在“今日方案”生成三餐，再创建购物清单。"));
        return;
    }
    ShoppingListDialog dialog(m_user.id, m_plan, this);
    dialog.exec();
}

void FridgeWidget::onRecommendExpiring()
{
    if (m_user.id <= 0)
        return;
    const QDate today = QDate::currentDate();
    QStringList expiring;
    for (const FridgeItem &item : FridgeDAO().listByUser(m_user.id)) {
        const QDate expiry = QDate::fromString(item.expiryDate, Qt::ISODate);
        if (expiry.isValid() && expiry >= today && today.daysTo(expiry) <= 3)
            expiring.append(item.foodName);
    }
    if (expiring.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("暂无临期食材"),
                                 QStringLiteral("当前没有3天内到期且仍可使用的食材。"));
        return;
    }
    showDishRecommendations(expiring);
    m_resultHint->setText(QStringLiteral("优先使用临期食材：%1。点击菜品查看详情。")
                              .arg(expiring.join(QStringLiteral("、"))));
}

void FridgeWidget::showDishRecommendations(const QStringList &foods)
{
    m_resultList->clear();
    const QList<FridgeMatchResult> ranked = FridgeClearEngine(m_user).rankRecipes(foods, 30);
    m_resultHint->setText(ranked.isEmpty() ? QStringLiteral("暂未匹配到菜谱。")
                                           : QStringLiteral("已推荐 %1 道可用菜品，点击查看详情。")
                                                 .arg(ranked.size()));
    for (const FridgeMatchResult &match : ranked) {
        addRecipeResultItem(
            m_resultList, match.recipe,
            QStringLiteral("%1 kcal · 命中 %2 样库存食材")
                .arg(int(qRound(match.recipe.totalCalories)))
                .arg(match.matchedCount));
    }
    m_inventoryTable->hide();
    m_resultPanel->show();
    m_showingResults = true;
    m_viewToggleBtn->setText(QStringLiteral("查看剩余食材"));
    m_viewToggleBtn->show();
}

void FridgeWidget::showDailyPlan(const RecommendResult &plan)
{
    m_resultList->clear();
    m_resultHint->setText(QStringLiteral("已生成清冰箱三餐，点击菜名查看详情。"));
    auto addMeal = [this](const MealSlot &slot) {
        auto *header = new QListWidgetItem(QStringLiteral("—— %1 ——").arg(slot.mealLabel), m_resultList);
        header->setFlags(Qt::NoItemFlags);
        header->setSizeHint(QSize(0, 34));
        QFont headerFont = header->font();
        headerFont.setBold(true);
        header->setFont(headerFont);
        header->setForeground(QColor(QStringLiteral("#28594A")));
        for (const Recipe &recipe : slot.dishes) {
            addRecipeResultItem(
                m_resultList, recipe,
                QStringLiteral("%1 kcal · 蛋白质 %2 g · 脂肪 %3 g")
                    .arg(int(qRound(recipe.totalCalories)))
                    .arg(recipe.totalProtein, 0, 'f', 1)
                    .arg(recipe.totalFat, 0, 'f', 1));
        }
    };
    addMeal(plan.breakfast);
    addMeal(plan.lunch);
    addMeal(plan.dinner);
    m_inventoryTable->hide();
    m_resultPanel->show();
    m_showingResults = true;
    m_viewToggleBtn->setText(QStringLiteral("查看剩余食材"));
    m_viewToggleBtn->show();
}

void FridgeWidget::onToggleView()
{
    if (!m_viewToggleBtn->isVisible())
        return;
    m_showingResults = !m_showingResults;
    m_resultPanel->setVisible(m_showingResults);
    m_inventoryTable->setVisible(!m_showingResults);
    m_viewToggleBtn->setText(m_showingResults ? QStringLiteral("查看剩余食材")
                                             : QStringLiteral("查看推荐结果"));
}

void FridgeWidget::onResultClicked(QListWidgetItem *item)
{
    if (item)
        openRecipeById(item->data(Qt::UserRole).toInt());
}

void FridgeWidget::openRecipeById(int recipeId)
{
    if (recipeId <= 0)
        return;
    const Recipe recipe = RecipeDAO().findById(recipeId);
    if (recipe.isValid())
        emit detailRequested(recipe);
}
