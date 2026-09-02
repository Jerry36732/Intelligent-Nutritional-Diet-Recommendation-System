#ifndef TAGCHIPGROUP_H
#define TAGCHIPGROUP_H

#include <QStringList>
#include <QWidget>

class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;

/** 多选标签组：支持搜索、折叠「更多」 */
class TagChipGroup : public QWidget
{
    Q_OBJECT

public:
    explicit TagChipGroup(const QString &title,
                          const QStringList &options,
                          QWidget *parent = nullptr,
                          int visibleLimit = 9,
                          bool enableSearch = true);

    void setSelected(const QStringList &selected);
    QStringList selected() const;
    void addOption(const QString &option, bool selected = false);
    void clearSelection();
    void setColumnCount(int columns);

signals:
    void selectionChanged();

private slots:
    void onSearchChanged(const QString &text);
    void toggleExpanded();

private:
    void rebuildVisibility();
    QPushButton *createButton(const QString &option);

    QLabel *m_titleLabel = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QGridLayout *m_grid = nullptr;
    QPushButton *m_moreBtn = nullptr;
    QList<QPushButton *> m_buttons;
    int m_visibleLimit = 9;
    int m_columnCount = 3;
    bool m_expanded = false;
    QString m_filter;
};

#endif // TAGCHIPGROUP_H
