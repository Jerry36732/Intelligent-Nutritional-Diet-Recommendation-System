#ifndef NPGENERATOR_H
#define NPGENERATOR_H

#include "../entities/RecommendResult.h"
#include "../entities/User.h"
#include "DiversityFilter.h"
#include "ScoredRecipe.h"

#include <QList>
#include <QSet>

/**
 * 营养计划（NP）生成器
 * 阶段二：在 RDSS 候选集上按 30/40/30 热量比例贪心组合一日三餐，并计算适配度
 */
class NPGenerator
{
public:
    NPGenerator(const User &user, const QList<ScoredRecipe> &candidates);

    void setDiversityFilter(const DiversityFilter &filter) { m_diversity = filter; }
    DiversityFilter &diversityFilter() { return m_diversity; }

    RecommendResult generateDailyPlan(const RecommendResult *previous = nullptr) const;
    double calculateFitness(const RecommendResult &plan) const;
    RecommendResult optimizePlan(const QList<RecommendResult> &plans) const;

private:
    Recipe pickFromRole(const QString &role,
                        double targetCal,
                        const QSet<int> &used,
                        bool allowReuseStaple = false) const;
    MealSlot composeBreakfast(double targetCal, QSet<int> &used) const;
    MealSlot composeMainMeal(const QString &label,
                             double targetCal,
                             QSet<int> &used,
                             bool forceWhiteRice = false) const;
    Recipe findWhiteRice() const;
    void ensureLunchOrDinnerHasWhiteRice(RecommendResult &plan) const;

    User m_user;
    QList<ScoredRecipe> m_candidates;
    DiversityFilter m_diversity;
    int m_dailyCal = 2000;
    double m_dailyProtein = 100.0;
};

#endif // NPGENERATOR_H
