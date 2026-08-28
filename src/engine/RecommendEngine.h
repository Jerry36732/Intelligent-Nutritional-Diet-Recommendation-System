#ifndef RECOMMENDENGINE_H
#define RECOMMENDENGINE_H

#include "../entities/RecommendResult.h"
#include "../entities/User.h"

#include <QList>
#include <QSet>
#include <QString>

/** 推荐引擎：RDSS 过滤打分 → NP 生成；候选不足时降级旧规则 */
class RecommendEngine
{
public:
    RecommendResult generatePlan(const User &user,
                                 const RecommendResult *previous = nullptr) const;

private:
    RecommendResult generateLegacyPlan(const User &user,
                                       const RecommendResult *previous) const;

    static double proteinPerKg(const QString &goal);
    static QStringList splitKeywords(const QString &text);
    static bool hitsAllergen(const Recipe &recipe, const QStringList &allergens);
    static double preferenceBonus(const Recipe &recipe, const QStringList &prefs);

    Recipe pickOne(const QList<Recipe> &candidates,
                   double targetCal,
                   double targetProtein,
                   const QSet<int> &excludeIds,
                   const QStringList &allergens,
                   const QStringList &prefs,
                   double *outScore) const;

    MealSlot composeBreakfast(const QList<Recipe> &pool,
                              const QList<Recipe> &drinks,
                              double targetCal,
                              double targetProtein,
                              QSet<int> &used,
                              const QStringList &allergens,
                              const QStringList &prefs) const;
    MealSlot composeMulti(const QString &label,
                          const QList<Recipe> &meats,
                          const QList<Recipe> &vegs,
                          const QList<Recipe> &staples,
                          const QList<Recipe> &soups,
                          double targetCal,
                          double targetProtein,
                          QSet<int> &used,
                          const QStringList &allergens,
                          const QStringList &prefs,
                          bool includeStaple) const;
};

#endif // RECOMMENDENGINE_H
