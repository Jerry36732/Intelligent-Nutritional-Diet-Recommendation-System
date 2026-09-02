#ifndef FLAVORFINGERPRINTSERVICE_H
#define FLAVORFINGERPRINTSERVICE_H

#include "../entities/Recipe.h"

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

struct FlavorFingerprint
{
    double sweet = 0.0;
    double sour = 0.0;
    double salty = 0.0;
    double spicy = 0.0;
    double umami = 0.0;
    double aroma = 0.0;
    double crispy = 0.0;
    double soft = 0.0;

    double value(int index) const;
    void setValue(int index, double value);
    static QStringList labels();
};

class FlavorFingerprintService
{
public:
    FlavorFingerprint forRecipe(const Recipe &recipe) const;
    FlavorFingerprint estimate(const QString &name,
                               const QList<QPair<QString, double>> &ingredients,
                               const QString &steps, double totalFat = 0.0,
                               double totalWeight = 0.0) const;
    bool persist(int recipeId, const FlavorFingerprint &fingerprint,
                 const QString &source = QStringLiteral("rule-v3-texture-baseline")) const;

    static int similarity(const FlavorFingerprint &before,
                          const FlavorFingerprint &after);
    static QString comparisonSummary(const FlavorFingerprint &before,
                                     const FlavorFingerprint &after);
};

#endif // FLAVORFINGERPRINTSERVICE_H
