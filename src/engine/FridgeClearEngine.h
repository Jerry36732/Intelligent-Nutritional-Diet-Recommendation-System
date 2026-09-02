#ifndef FRIDGECLEARENGINE_H
#define FRIDGECLEARENGINE_H

#include "../entities/Recipe.h"
#include "../entities/RecommendResult.h"
#include "../entities/User.h"

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

struct FridgeMatchResult
{
    Recipe recipe;
    int matchedCount = 0;
    int totalIngredients = 0;
    QStringList matchedNames;
    QStringList missingNames; // 冰箱没有、菜谱需要的食材（允许外购补齐）
    double matchRatio = 0.0;
    double score = 0.0;
};

/**
 * 清冰箱：优先使用冰箱食材匹配菜谱。
 * 核心：尽量用上冰箱里的料，而不是「只能」用冰箱里的料——缺的可外购补齐。
 */
class FridgeClearEngine
{
public:
    explicit FridgeClearEngine(const User &user);

    QList<FridgeMatchResult> rankRecipes(const QStringList &fridgeFoods, int limit = 24) const;

    /** 食材充足时生成结合冰箱的一日三餐；失败则 valid=false */
    RecommendResult generateDailyPlan(const QStringList &fridgeFoods) const;

private:
    static bool nameHits(const QString &haystack, const QString &needle);
    FridgeMatchResult scoreRecipe(const Recipe &recipe, const QStringList &fridgeFoods) const;
    Recipe pickBest(const QList<FridgeMatchResult> &ranked,
                    const QSet<int> &used,
                    const QString &rolePrefer) const;

    User m_user;
};

#endif // FRIDGECLEARENGINE_H
