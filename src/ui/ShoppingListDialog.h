#ifndef SHOPPINGLISTDIALOG_H
#define SHOPPINGLISTDIALOG_H

#include "../entities/RecommendResult.h"
#include "../services/ShoppingListService.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QTableWidget;

class ShoppingListDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShoppingListDialog(int userId, const RecommendResult &plan,
                                QWidget *parent = nullptr);

private slots:
    void rebuild();
    void copyForSharing();
    void exportList();

private:
    QString scopeLabel() const;

    int m_userId = 0;
    RecommendResult m_plan;
    QList<ShoppingListItem> m_items;
    QComboBox *m_scope = nullptr;
    QLabel *m_summary = nullptr;
    QTableWidget *m_table = nullptr;
};

#endif // SHOPPINGLISTDIALOG_H
