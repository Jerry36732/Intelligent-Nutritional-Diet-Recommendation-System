#ifndef TAGCHIPGROUP_H
#define TAGCHIPGROUP_H

#include <QStringList>
#include <QWidget>

class QGridLayout;
class QLabel;

/** 多选标签按钮组：用于饮食偏好 / 过敏 / 不耐受等档案维度 */
class TagChipGroup : public QWidget
{
    Q_OBJECT

public:
    explicit TagChipGroup(const QString &title,
                          const QStringList &options,
                          QWidget *parent = nullptr);

    void setSelected(const QStringList &selected);
    QStringList selected() const;

signals:
    void selectionChanged();

private:
    QList<class QPushButton *> m_buttons;
};

#endif // TAGCHIPGROUP_H
