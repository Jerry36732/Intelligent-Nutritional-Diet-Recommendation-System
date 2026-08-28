#ifndef SCOREDRECIPE_H
#define SCOREDRECIPE_H

#include "../entities/Recipe.h"

/** RDSS 处理后的候选食谱（含营养增强得分） */
struct ScoredRecipe
{
    Recipe recipe;
    double nutritionBoost = 0.0; // 营养增强得分
    double baseScore = 0.0;      // 综合分（热量/蛋白等）
    double totalScore() const { return baseScore + nutritionBoost; }
};

#endif // SCOREDRECIPE_H
