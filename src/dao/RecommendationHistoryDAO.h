#ifndef RECOMMENDATIONHISTORYDAO_H
#define RECOMMENDATIONHISTORYDAO_H

#include "../entities/RecommendResult.h"

#include <QSet>
#include <QString>

class RecommendationHistoryDAO
{
public:
    /** 写入一日方案到历史（保留近 7 天） */
    bool recordPlan(int userId, const RecommendResult &plan);

    /** 近 days 天出现过的 recipe_id */
    QSet<int> recentRecipeIds(int userId, int days = 3) const;

    /** 清理 7 天前记录 */
    void purgeOlderThan(int userId, int keepDays = 7) const;
};

#endif // RECOMMENDATIONHISTORYDAO_H
