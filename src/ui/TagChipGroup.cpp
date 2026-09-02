#include "TagChipGroup.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVBoxLayout>

TagChipGroup::TagChipGroup(const QString &title,
                           const QStringList &options,
                           QWidget *parent,
                           int visibleLimit,
                           bool enableSearch)
    : QWidget(parent)
    , m_visibleLimit(qMax(3, visibleLimit))
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setObjectName(QStringLiteral("TagGroupTitle"));
    m_titleLabel->setVisible(!title.trimmed().isEmpty());
    header->addWidget(m_titleLabel, 1);

    if (enableSearch) {
        m_searchEdit = new QLineEdit(this);
        m_searchEdit->setObjectName(QStringLiteral("TagSearchInput"));
        m_searchEdit->setPlaceholderText(QStringLiteral("搜索选项…"));
        m_searchEdit->setClearButtonEnabled(true);
        m_searchEdit->setMaximumWidth(160);
        m_searchEdit->setMinimumHeight(28);
        connect(m_searchEdit, &QLineEdit::textChanged, this, &TagChipGroup::onSearchChanged);
        header->addWidget(m_searchEdit);
    }
    if (!title.trimmed().isEmpty() || enableSearch)
        root->addLayout(header);

    m_grid = new QGridLayout;
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(8);
    m_grid->setVerticalSpacing(8);

    for (const QString &opt : options)
        m_buttons.append(createButton(opt));
    root->addLayout(m_grid);

    m_moreBtn = new QPushButton(this);
    m_moreBtn->setObjectName(QStringLiteral("TagMoreButton"));
    m_moreBtn->setCursor(Qt::PointingHandCursor);
    m_moreBtn->setFlat(true);
    connect(m_moreBtn, &QPushButton::clicked, this, &TagChipGroup::toggleExpanded);
    root->addWidget(m_moreBtn, 0, Qt::AlignLeft);

    // 仅在此处完成首次布局，避免“先全部 add 再重复 add”导致网格项损坏
    rebuildVisibility();
}

QPushButton *TagChipGroup::createButton(const QString &option)
{
    auto *btn = new QPushButton(option, this);
    btn->setCheckable(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setProperty("class", QVariant(QStringLiteral("TagChip")));
    btn->setProperty("tagValue", option);
    btn->setMinimumHeight(32);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(btn, &QPushButton::toggled, this, [this, btn](bool checked) {
        const QString value = btn->property("tagValue").toString();
        const bool isNone = value.contains(QStringLiteral("无相关情况"))
                            || value.contains(QStringLiteral("暂无"));
        if (checked && isNone) {
            for (QPushButton *other : m_buttons) {
                if (other == btn || !other->isChecked())
                    continue;
                const QSignalBlocker blocker(other);
                other->setChecked(false);
            }
        } else if (checked) {
            for (QPushButton *other : m_buttons) {
                const QString otherValue = other->property("tagValue").toString();
                if (other != btn && other->isChecked()
                    && (otherValue.contains(QStringLiteral("无相关情况"))
                        || otherValue.contains(QStringLiteral("暂无")))) {
                    const QSignalBlocker blocker(other);
                    other->setChecked(false);
                }
            }
        }
        emit selectionChanged();
    });
    return btn;
}

void TagChipGroup::addOption(const QString &option, bool selected)
{
    const QString value = option.trimmed();
    if (value.isEmpty())
        return;
    for (QPushButton *button : m_buttons) {
        if (button->property("tagValue").toString() == value) {
            button->setChecked(selected || button->isChecked());
            rebuildVisibility();
            return;
        }
    }
    auto *button = createButton(value);
    button->setChecked(selected);
    m_buttons.append(button);
    rebuildVisibility();
}

void TagChipGroup::clearSelection()
{
    for (QPushButton *button : m_buttons)
        button->setChecked(false);
    rebuildVisibility();
}

void TagChipGroup::setColumnCount(int columns)
{
    m_columnCount = qMax(1, columns);
    for (int column = 0; column < m_columnCount; ++column)
        m_grid->setColumnStretch(column, 1);
    rebuildVisibility();
}

void TagChipGroup::setSelected(const QStringList &selected)
{
    for (QPushButton *btn : m_buttons) {
        const QString v = btn->property("tagValue").toString();
        const bool on = selected.contains(v);
        btn->blockSignals(true);
        btn->setChecked(on);
        btn->blockSignals(false);
    }
    rebuildVisibility();
}

QStringList TagChipGroup::selected() const
{
    QStringList out;
    for (QPushButton *btn : m_buttons) {
        if (btn->isChecked())
            out.append(btn->property("tagValue").toString());
    }
    return out;
}

void TagChipGroup::onSearchChanged(const QString &text)
{
    m_filter = text.trimmed();
    if (!m_filter.isEmpty())
        m_expanded = true;
    rebuildVisibility();
}

void TagChipGroup::toggleExpanded()
{
    m_expanded = !m_expanded;
    rebuildVisibility();
}

void TagChipGroup::rebuildVisibility()
{
    // 先清空布局项（不销毁按钮），再按可见性重新排布，避免同一格子多重占用
    while (QLayoutItem *item = m_grid->takeAt(0))
        delete item;

    const bool filtering = !m_filter.isEmpty();
    int shownMatching = 0;
    const int columns = qMax(1, m_columnCount);
    int col = 0;
    int row = 0;

    for (QPushButton *btn : m_buttons) {
        const QString v = btn->property("tagValue").toString();
        const bool matches = !filtering || v.contains(m_filter, Qt::CaseInsensitive);
        bool visible = false;
        if (matches) {
            if (filtering || m_expanded || shownMatching < m_visibleLimit || btn->isChecked())
                visible = true;
        }
        if (btn->isChecked())
            visible = true;

        btn->setVisible(visible);
        if (!visible)
            continue;

        m_grid->addWidget(btn, row, col);
        ++col;
        if (col >= columns) {
            col = 0;
            ++row;
        }
        if (matches)
            ++shownMatching;
    }

    const int hidden = filtering ? 0 : qMax(0, m_buttons.size() - m_visibleLimit);
    if (filtering) {
        m_moreBtn->setVisible(false);
    } else if (m_buttons.size() > m_visibleLimit) {
        m_moreBtn->setVisible(true);
        m_moreBtn->setText(m_expanded
                               ? QStringLiteral("收起选项")
                               : QStringLiteral("更多选项（+%1）").arg(hidden));
    } else {
        m_moreBtn->setVisible(false);
    }
}
