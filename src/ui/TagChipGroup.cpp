#include "TagChipGroup.h"

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

TagChipGroup::TagChipGroup(const QString &title, const QStringList &options, QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    auto *titleLabel = new QLabel(title, this);
    titleLabel->setObjectName(QStringLiteral("TagGroupTitle"));
    root->addWidget(titleLabel);

    auto *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);

    int col = 0;
    int row = 0;
    constexpr int kCols = 3;
    for (const QString &opt : options) {
        auto *btn = new QPushButton(opt, this);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("class", QVariant(QStringLiteral("TagChip")));
        btn->setProperty("tagValue", opt);
        btn->setMinimumHeight(32);
        btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        connect(btn, &QPushButton::toggled, this, [this](bool) { emit selectionChanged(); });
        m_buttons.append(btn);
        grid->addWidget(btn, row, col);
        ++col;
        if (col >= kCols) {
            col = 0;
            ++row;
        }
    }
    root->addLayout(grid);
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
