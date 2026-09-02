#ifndef SHOPPINGLISTSERVICE_H
#define SHOPPINGLISTSERVICE_H

#include "../entities/RecommendResult.h"

#include <QList>
#include <QString>

struct ShoppingListItem
{
    QString name;
    QString category;
    double plannedGrams = 0.0;
    double fridgeGrams = 0.0;
    double buyGrams = 0.0;
    bool spice = false;
};

class ShoppingListService
{
public:
    QList<ShoppingListItem> build(int userId, const RecommendResult &plan,
                                  int planDays = 1) const;
    static QString toShareText(const QList<ShoppingListItem> &items,
                               const QString &scopeLabel);
    static QString normalizedIngredientName(const QString &name);
    static bool isCommonPantrySeasoning(const QString &name);
    static bool isSpice(const QString &name);

private:
    static QString categoryFor(const QString &name);
    static double fridgeGrams(const QString &name, double quantity, const QString &unit);
};

#endif // SHOPPINGLISTSERVICE_H
