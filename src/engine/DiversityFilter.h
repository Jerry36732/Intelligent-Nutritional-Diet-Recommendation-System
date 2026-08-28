#ifndef DIVERSITYFILTER_H
#define DIVERSITYFILTER_H

#include "../entities/RecommendResult.h"
#include "../entities/Recipe.h"

#include <QSet>
#include <QStringList>

/**
 * 膳食多样性过滤器（可插拔）
 * 规则1：同一天内食谱不重复
 * 规则2：近 3 天内同一食谱不重复
 * 规则3：当日菜品种类尽量多样（≥3）
 */
class DiversityFilter
{
public:
    explicit DiversityFilter(bool enabled = true);

    void setEnabled(bool on) { m_enabled = on; }
    bool isEnabled() const { return m_enabled; }

    /** 近 3 天已推荐过的 recipe id */
    void setRecentRecipeIds(const QSet<int> &ids) { m_recentIds = ids; }

    /** 判断候选是否可用（相对当日已用 + 近期历史） */
    bool allows(const Recipe &recipe, const QSet<int> &usedToday, bool allowStapleReuse = false) const;

    /** 方案是否满足多样性底线 */
    bool planLooksDiverse(const RecommendResult &plan) const;

    /** 从文本中提取粗粒度食物关键词（香蕉/菠菜等）用于同日避重 */
    static QStringList foodTokens(const Recipe &recipe);

private:
    bool m_enabled = true;
    QSet<int> m_recentIds;
};

#endif // DIVERSITYFILTER_H
