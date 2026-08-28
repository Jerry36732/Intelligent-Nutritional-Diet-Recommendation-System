#ifndef RDSSENGINE_H
#define RDSSENGINE_H

#include "../entities/Recipe.h"
#include "../entities/User.h"
#include "ScoredRecipe.h"

#include <QList>
#include <QMap>
#include <QStringList>

/**
 * 推理决策支持系统（RDSS）
 * 阶段一：硬性过滤（过敏/不耐受/医疗）+ 营养缺乏主动推荐打分
 */
class RDSSEngine
{
public:
    explicit RDSSEngine(const User &user);

    QList<Recipe> filterByAllergies(const QList<Recipe> &recipes) const;
    QList<Recipe> filterByIntolerances(const QList<Recipe> &recipes) const;
    QList<Recipe> filterByMedicalConditions(const QList<Recipe> &recipes) const;
    QList<ScoredRecipe> promoteByDeficiencies(const QList<Recipe> &recipes) const;

    /** 完整 RDSS：过滤 → 打分，按 totalScore 降序 */
    QList<ScoredRecipe> process(const QList<Recipe> &recipes) const;

    /** 展开过敏原到具体食物关键词（如 豆制品→豆腐/豆浆…） */
    QStringList expandedAvoidKeywords() const;

    static QString recipeBlob(const Recipe &recipe);
    static bool containsAny(const QString &blob, const QStringList &keywords);

    /** 内置兜底映射（数据库未就绪时仍可用） */
    static QMap<QString, QStringList> builtinAllergenMap();
    static QMap<QString, QStringList> builtinNutrientFoodMap();
    static QMap<QString, QString> builtinDeficiencyNutrientMap();
    static QMap<QString, QStringList> builtinMedicalRestrictMap();

private:
    void loadKnowledge();
    QStringList keywordsForAllergen(const QString &allergen) const;
    QStringList keywordsForIntolerance(const QString &item) const;
    double deficiencyBoost(const Recipe &recipe) const;

    User m_user;
    QMap<QString, QStringList> m_allergenMap;
    QMap<QString, QStringList> m_nutrientFoodMap;
    QMap<QString, QString> m_deficiencyNutrient;
    QMap<QString, QStringList> m_medicalBan;
};

#endif // RDSSENGINE_H
