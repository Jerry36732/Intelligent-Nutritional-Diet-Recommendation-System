#ifndef INGREDIENTMEASURESERVICE_H
#define INGREDIENTMEASURESERVICE_H

#include <QString>

struct IngredientMeasureEstimate
{
    QString ingredientName;
    QString quantityText;
    double grams = 0.0;
    bool valid = false;
    bool estimated = false;
};

class IngredientMeasureService
{
public:
    static IngredientMeasureEstimate parse(const QString &line);
    static double gramsPerUnit(const QString &ingredientName, const QString &unit);
};

#endif // INGREDIENTMEASURESERVICE_H
