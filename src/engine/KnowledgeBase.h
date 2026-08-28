#ifndef KNOWLEDGEBASE_H
#define KNOWLEDGEBASE_H

#include "../entities/Food.h"
#include "../entities/RecommendResult.h"
#include "../entities/User.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

/** NAct 风格结构化知识库（SQLite 热更新） */
class KnowledgeBase
{
public:
    KnowledgeBase() = default;

    /** 从数据库重新加载缓存（热更新） */
    void reload();

    QList<Food> getFoodsRichInNutrient(const QString &nutrientName) const;
    QList<Food> getFoodsToAvoidForAllergy(const QString &allergen) const;
    QList<Food> getFoodsToAvoidForDisease(const QString &disease) const;
    QStringList getDietaryGuidelines(const User &user) const;

    /** 根据用户画像与当前方案生成推荐理由 */
    QStringList buildRecommendationReasons(const User &user, const RecommendResult &plan) const;

    QStringList foodNamesRichIn(const QString &nutrientName, int limit = 8) const;
    QStringList avoidKeywordsForAllergy(const QString &allergen) const;

private:
    bool m_loaded = false;
    void ensureLoaded() const;
    void loadCache();

    // mutable 缓存：允许 const 查询时惰性加载
    mutable QHash<QString, QList<int>> m_nutrientToFoodIds;
    mutable QHash<QString, QList<int>> m_allergenToFoodIds;
    mutable QHash<QString, QList<int>> m_deficiencyToFoodIds;
    mutable QHash<QString, QStringList> m_diseaseBanKeywords;
    mutable QHash<int, Food> m_foodById;
};

#endif // KNOWLEDGEBASE_H
