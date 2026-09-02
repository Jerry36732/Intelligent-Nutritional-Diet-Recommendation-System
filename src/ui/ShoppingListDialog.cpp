#include "ShoppingListDialog.h"
#include "UiAssets.h"
#include "../services/ShoppingListExportService.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QFileDialog>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
QString formatGrams(double grams)
{
    if (grams >= 1000.0)
        return QStringLiteral("%1 kg").arg(grams / 1000.0, 0, 'f', 1);
    return QStringLiteral("%1 g").arg(qMax(1, int(qCeil(grams))));
}
}

ShoppingListDialog::ShoppingListDialog(int userId, const RecommendResult &plan, QWidget *parent)
    : QDialog(parent), m_userId(userId), m_plan(plan)
{
    setObjectName(QStringLiteral("ShoppingListDialog"));
    setWindowTitle(QStringLiteral("智能购物清单"));
    setModal(true);
    resize(720, 590);
    setMinimumSize(660, 520);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(26, 22, 26, 22);
    root->setSpacing(14);

    auto *heading = new QHBoxLayout;
    auto *titles = new QVBoxLayout;
    titles->setSpacing(3);
    auto *title = new QLabel(QStringLiteral("智能购物清单"), this);
    title->setObjectName(QStringLiteral("DialogTitle"));
    title->setFont(UiAssets::titleFont(22));
    auto *subtitle = new QLabel(
        QStringLiteral("已按食谱汇总原料并扣除冰箱库存；常备调味料不会列入。"), this);
    subtitle->setObjectName(QStringLiteral("DialogSubtitle"));
    titles->addWidget(title);
    titles->addWidget(subtitle);
    m_scope = new QComboBox(this);
    m_scope->addItem(QStringLiteral("今日方案"), 1);
    m_scope->addItem(QStringLiteral("未来7天（按当前方案）"), 7);
    m_scope->setFixedWidth(210);
    heading->addLayout(titles, 1);
    heading->addWidget(m_scope, 0, Qt::AlignTop);
    root->addLayout(heading);

    auto *notice = new QFrame(this);
    notice->setObjectName(QStringLiteral("ShoppingListNotice"));
    auto *noticeLayout = new QHBoxLayout(notice);
    noticeLayout->setContentsMargins(12, 9, 12, 9);
    noticeLayout->setSpacing(9);
    auto *noticeIcon = UiAssets::createIconLabel(
        notice, QStringLiteral("fridge"), 19, QColor(QStringLiteral("#28594A")));
    m_summary = new QLabel(notice);
    m_summary->setWordWrap(true);
    noticeLayout->addWidget(noticeIcon);
    noticeLayout->addWidget(m_summary, 1);
    root->addWidget(notice);

    m_table = new QTableWidget(this);
    m_table->setObjectName(QStringLiteral("ShoppingListTable"));
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({QStringLiteral("购买"), QStringLiteral("分类"),
                                        QStringLiteral("食材"), QStringLiteral("方案需要"),
                                        QStringLiteral("实际购买")});
    m_table->verticalHeader()->hide();
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    root->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout;
    auto *rule = new QLabel(QStringLiteral("保留八角、桂皮等香料；不列盐、味精等常备调味料。"), this);
    rule->setObjectName(QStringLiteral("DialogHint"));
    auto *copy = new QPushButton(QStringLiteral("复制分享"), this);
    copy->setProperty("class", QStringLiteral("GhostButton"));
    UiAssets::setButtonIcon(copy, QStringLiteral("send"), 17);
    auto *save = new QPushButton(QStringLiteral("导出清单"), this);
    save->setProperty("class", QStringLiteral("PrimaryButton"));
    UiAssets::setButtonIcon(save, QStringLiteral("basket"), 17, QColor(Qt::white));
    auto *close = new QPushButton(QStringLiteral("关闭"), this);
    close->setProperty("class", QStringLiteral("GhostButton"));
    actions->addWidget(rule, 1);
    actions->addWidget(copy);
    actions->addWidget(save);
    actions->addWidget(close);
    root->addLayout(actions);

    connect(m_scope, &QComboBox::currentIndexChanged, this, &ShoppingListDialog::rebuild);
    connect(copy, &QPushButton::clicked, this, &ShoppingListDialog::copyForSharing);
    connect(save, &QPushButton::clicked, this, &ShoppingListDialog::exportList);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    rebuild();
}

QString ShoppingListDialog::scopeLabel() const
{
    return m_scope ? m_scope->currentText() : QStringLiteral("今日方案");
}

void ShoppingListDialog::rebuild()
{
    const int days = m_scope->currentData().toInt();
    m_items = ShoppingListService().build(m_userId, m_plan, days);
    m_table->setRowCount(m_items.size());
    double totalBuy = 0.0;
    double deducted = 0.0;
    for (int row = 0; row < m_items.size(); ++row) {
        const ShoppingListItem &item = m_items.at(row);
        totalBuy += item.buyGrams;
        deducted += item.fridgeGrams;
        auto *check = new QTableWidgetItem;
        check->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        check->setCheckState(Qt::Unchecked);
        check->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 0, check);
        m_table->setItem(row, 1, new QTableWidgetItem(item.category));
        auto *name = new QTableWidgetItem(item.name);
        if (item.spice)
            name->setToolTip(QStringLiteral("香料按要求保留在购物清单中"));
        m_table->setItem(row, 2, name);
        m_table->setItem(row, 3, new QTableWidgetItem(formatGrams(item.plannedGrams)));
        auto *buy = new QTableWidgetItem(formatGrams(item.buyGrams));
        buy->setForeground(QColor(QStringLiteral("#28594A")));
        m_table->setItem(row, 4, buy);
        for (int col = 0; col < 5; ++col) {
            m_table->item(row, col)->setTextAlignment(
                col >= 3 ? Qt::AlignRight | Qt::AlignVCenter : Qt::AlignLeft | Qt::AlignVCenter);
        }
        m_table->setRowHeight(row, 42);
    }
    m_summary->setText(m_items.isEmpty()
                           ? QStringLiteral("冰箱库存已覆盖当前方案，无需额外购买。")
                           : QStringLiteral("需购买 %1 类食材，约 %2；已从冰箱自动扣除 %3。")
                                 .arg(m_items.size()).arg(formatGrams(totalBuy)).arg(formatGrams(deducted)));
}

void ShoppingListDialog::copyForSharing()
{
    QApplication::clipboard()->setText(ShoppingListService::toShareText(m_items, scopeLabel()));
    QMessageBox::information(this, QStringLiteral("已复制"),
                             QStringLiteral("购物清单已复制，可直接粘贴到微信或其他应用分享。"));
}

void ShoppingListDialog::exportList()
{
    QString selectedFilter;
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出购物清单"), QStringLiteral("膳衡购物清单.pdf"),
        QStringLiteral("PDF 文件 (*.pdf);;Word 文档 (*.docx);;Excel 工作簿 (*.xlsx);;文本文件 (*.txt)"),
        &selectedFilter);
    if (path.isEmpty())
        return;

    ShoppingListExportService::Format format = ShoppingListExportService::Format::Pdf;
    if (selectedFilter.contains(QStringLiteral("Word")) || path.endsWith(QStringLiteral(".docx"), Qt::CaseInsensitive))
        format = ShoppingListExportService::Format::Word;
    else if (selectedFilter.contains(QStringLiteral("Excel")) || path.endsWith(QStringLiteral(".xlsx"), Qt::CaseInsensitive))
        format = ShoppingListExportService::Format::Excel;
    else if (selectedFilter.contains(QStringLiteral("文本")) || path.endsWith(QStringLiteral(".txt"), Qt::CaseInsensitive))
        format = ShoppingListExportService::Format::Text;
    const QString suffix = QStringLiteral(".") + ShoppingListExportService::extension(format);
    if (!path.endsWith(suffix, Qt::CaseInsensitive))
        path += suffix;

    QString error;
    if (!ShoppingListExportService::exportList(path, format, m_items, scopeLabel(), &error)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             error.isEmpty() ? QStringLiteral("文件保存失败。") : error);
        return;
    }
    QMessageBox::information(this, QStringLiteral("导出完成"),
                             QStringLiteral("购物清单已保存为 %1 文件。")
                                 .arg(ShoppingListExportService::extension(format).toUpper()));
}
