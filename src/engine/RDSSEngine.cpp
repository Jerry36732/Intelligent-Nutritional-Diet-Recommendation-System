#include "RDSSEngine.h"
#include "ScoreCalculator.h"
#include "../dao/DatabaseManager.h"

#include <QSqlQuery>
#include <QtMath>
#include <algorithm>

QMap<QString, QStringList> RDSSEngine::builtinAllergenMap()
{
    return {
        {QStringLiteral("豆制品"),
         {QStringLiteral("豆腐"), QStringLiteral("豆浆"), QStringLiteral("豆皮"), QStringLiteral("腐竹"),
          QStringLiteral("豆芽"), QStringLiteral("豆腐干"), QStringLiteral("豆花"), QStringLiteral("千张"),
          QStringLiteral("黄豆"), QStringLiteral("大豆"), QStringLiteral("毛豆"), QStringLiteral("豆豉"),
          QStringLiteral("豆干"), QStringLiteral("内酯豆腐"), QStringLiteral("豆制品")}},
        {QStringLiteral("大豆"),
         {QStringLiteral("大豆"), QStringLiteral("黄豆"), QStringLiteral("豆腐"), QStringLiteral("豆浆"),
          QStringLiteral("豆皮"), QStringLiteral("腐竹")}},
        {QStringLiteral("花生"), {QStringLiteral("花生"), QStringLiteral("花生酱"), QStringLiteral("花生油")}},
        {QStringLiteral("坚果"),
         {QStringLiteral("核桃"), QStringLiteral("杏仁"), QStringLiteral("腰果"), QStringLiteral("榛子"),
          QStringLiteral("开心果"), QStringLiteral("坚果")}},
        {QStringLiteral("牛奶"),
         {QStringLiteral("牛奶"), QStringLiteral("牛乳"), QStringLiteral("奶粉"), QStringLiteral("奶油"),
          QStringLiteral("奶酪"), QStringLiteral("芝士"), QStringLiteral("黄油"), QStringLiteral("炼乳")}},
        {QStringLiteral("鸡蛋"), {QStringLiteral("鸡蛋"), QStringLiteral("蛋清"), QStringLiteral("蛋黄"), QStringLiteral("蛋液")}},
        {QStringLiteral("海鲜"),
         {QStringLiteral("海鲜"), QStringLiteral("鱼"), QStringLiteral("虾"), QStringLiteral("蟹"),
          QStringLiteral("贝"), QStringLiteral("鱿鱼"), QStringLiteral("墨鱼"), QStringLiteral("带鱼"),
          QStringLiteral("鲈鱼"), QStringLiteral("甲鱼")}},
        {QStringLiteral("贝类"), {QStringLiteral("贝"), QStringLiteral("蛤"), QStringLiteral("蚝"), QStringLiteral("扇贝"), QStringLiteral("牡蛎")}},
        {QStringLiteral("麸质（小麦）"),
         {QStringLiteral("小麦"), QStringLiteral("面粉"), QStringLiteral("面条"), QStringLiteral("馒头"),
          QStringLiteral("面包"), QStringLiteral("饺子"), QStringLiteral("麸质")}},
        {QStringLiteral("芝麻"), {QStringLiteral("芝麻"), QStringLiteral("香油"), QStringLiteral("麻酱")}},
        {QStringLiteral("猕猴桃"), {QStringLiteral("猕猴桃"), QStringLiteral("奇异果")}},
    };
}

QMap<QString, QStringList> RDSSEngine::builtinNutrientFoodMap()
{
    return {
        {QStringLiteral("铁"),
         {QStringLiteral("菠菜"), QStringLiteral("猪肝"), QStringLiteral("牛肉"), QStringLiteral("鸭血"),
          QStringLiteral("黑木耳"), QStringLiteral("红肉"), QStringLiteral("瘦肉")}},
        {QStringLiteral("钙"),
         {QStringLiteral("牛奶"), QStringLiteral("酸奶"), QStringLiteral("豆腐"), QStringLiteral("芝麻"),
          QStringLiteral("虾皮"), QStringLiteral("小白菜")}},
        {QStringLiteral("维生素D"),
         {QStringLiteral("蛋黄"), QStringLiteral("蘑菇"), QStringLiteral("鱼"), QStringLiteral("牛奶")}},
        {QStringLiteral("维生素B12"),
         {QStringLiteral("牛肉"), QStringLiteral("猪肝"), QStringLiteral("鸡蛋"), QStringLiteral("牛奶"),
          QStringLiteral("鱼")}},
        {QStringLiteral("蛋白质"),
         {QStringLiteral("鸡胸"), QStringLiteral("牛肉"), QStringLiteral("鸡蛋"), QStringLiteral("鱼"),
          QStringLiteral("虾"), QStringLiteral("豆腐"), QStringLiteral("瘦肉")}},
    };
}

QMap<QString, QString> RDSSEngine::builtinDeficiencyNutrientMap()
{
    return {
        {QStringLiteral("缺铁"), QStringLiteral("铁")},
        {QStringLiteral("缺钙"), QStringLiteral("钙")},
        {QStringLiteral("缺维生素D"), QStringLiteral("维生素D")},
        {QStringLiteral("缺维生素B12"), QStringLiteral("维生素B12")},
        {QStringLiteral("蛋白质不足"), QStringLiteral("蛋白质")},
        {QStringLiteral("贫血"), QStringLiteral("铁")},
    };
}

QMap<QString, QStringList> RDSSEngine::builtinMedicalRestrictMap()
{
    return {
        {QStringLiteral("2型糖尿病"),
         {QStringLiteral("糖水"), QStringLiteral("甜品"), QStringLiteral("蛋糕"), QStringLiteral("奶茶"),
          QStringLiteral("红糖"), QStringLiteral("冰糖"), QStringLiteral("蜜")}},
        {QStringLiteral("糖尿病"),
         {QStringLiteral("糖水"), QStringLiteral("甜品"), QStringLiteral("蛋糕"), QStringLiteral("奶茶"),
          QStringLiteral("红糖"), QStringLiteral("冰糖")}},
        {QStringLiteral("高血压"),
         {QStringLiteral("咸鱼"), QStringLiteral("腊肉"), QStringLiteral("香肠"), QStringLiteral("腌"),
          QStringLiteral("酱肉")}},
        {QStringLiteral("高血脂"),
         {QStringLiteral("肥肉"), QStringLiteral("油炸"), QStringLiteral("五花"), QStringLiteral("奶油")}},
        {QStringLiteral("心血管疾病"),
         {QStringLiteral("肥肉"), QStringLiteral("油炸"), QStringLiteral("动物油")}},
        {QStringLiteral("肾病"),
         {QStringLiteral("高盐"), QStringLiteral("浓汤"), QStringLiteral("动物内脏")}},
    };
}

RDSSEngine::RDSSEngine(const User &user)
    : m_user(user)
{
    loadKnowledge();
}

void RDSSEngine::loadKnowledge()
{
    m_allergenMap = builtinAllergenMap();
    m_nutrientFoodMap = builtinNutrientFoodMap();
    m_deficiencyNutrient = builtinDeficiencyNutrientMap();
    m_medicalBan = builtinMedicalRestrictMap();

    if (!DatabaseManager::getInstance().isOpen())
        return;

    QSqlDatabase db = DatabaseManager::getInstance().database();
    QSqlQuery q(db);

    if (q.exec(QStringLiteral("SELECT allergen, food_keyword FROM allergen_food_map"))) {
        while (q.next()) {
            m_allergenMap[q.value(0).toString()].append(q.value(1).toString());
        }
    }
    if (q.exec(QStringLiteral("SELECT nutrient, food_keyword FROM nutrient_food_map"))) {
        while (q.next()) {
            m_nutrientFoodMap[q.value(0).toString()].append(q.value(1).toString());
        }
    }
    if (q.exec(QStringLiteral("SELECT deficiency, nutrient FROM deficiency_nutrient_map"))) {
        while (q.next()) {
            m_deficiencyNutrient.insert(q.value(0).toString(), q.value(1).toString());
        }
    }
    if (q.exec(QStringLiteral("SELECT condition_name, ban_keyword FROM medical_food_restrict"))) {
        while (q.next()) {
            m_medicalBan[q.value(0).toString()].append(q.value(1).toString());
        }
    }

    // 去重
    for (auto it = m_allergenMap.begin(); it != m_allergenMap.end(); ++it)
        it.value().removeDuplicates();
    for (auto it = m_nutrientFoodMap.begin(); it != m_nutrientFoodMap.end(); ++it)
        it.value().removeDuplicates();
    for (auto it = m_medicalBan.begin(); it != m_medicalBan.end(); ++it)
        it.value().removeDuplicates();
}

QString RDSSEngine::recipeBlob(const Recipe &recipe)
{
    QString blob = recipe.name + QLatin1Char(' ') + recipe.steps;
    for (const RecipeIngredient &ing : recipe.ingredients)
        blob += QLatin1Char(' ') + ing.foodName;
    return blob;
}

bool RDSSEngine::containsAny(const QString &blob, const QStringList &keywords)
{
    for (const QString &kw : keywords) {
        const QString k = kw.trimmed();
        if (!k.isEmpty() && blob.contains(k, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

QStringList RDSSEngine::keywordsForAllergen(const QString &allergen) const
{
    QStringList out;
    const QString a = allergen.trimmed();
    if (a.isEmpty())
        return out;
    out.append(a);
    if (m_allergenMap.contains(a))
        out += m_allergenMap.value(a);
    // 模糊：用户写「豆制品过敏」时 allergen 可能是「豆制品」
    for (auto it = m_allergenMap.constBegin(); it != m_allergenMap.constEnd(); ++it) {
        if (a.contains(it.key()) || it.key().contains(a))
            out += it.value();
    }
    out.removeDuplicates();
    return out;
}

QStringList RDSSEngine::keywordsForIntolerance(const QString &item) const
{
    const QString t = item.trimmed();
    if (t == QLatin1String("乳糖"))
        return {QStringLiteral("奶"), QStringLiteral("牛奶"), QStringLiteral("酸奶"), QStringLiteral("奶酪"),
                QStringLiteral("奶油"), QStringLiteral("乳糖")};
    if (t == QLatin1String("麸质"))
        return {QStringLiteral("小麦"), QStringLiteral("面粉"), QStringLiteral("面条"), QStringLiteral("馒头"),
                QStringLiteral("面包"), QStringLiteral("麸质")};
    if (t == QLatin1String("果糖"))
        return {QStringLiteral("蜂蜜"), QStringLiteral("果汁"), QStringLiteral("果糖")};
    if (t == QLatin1String("FODMAPs"))
        return {QStringLiteral("洋葱"), QStringLiteral("大蒜"), QStringLiteral("豆类"), QStringLiteral("苹果")};
    return {t};
}

QStringList RDSSEngine::expandedAvoidKeywords() const
{
    QStringList out = m_user.avoidanceKeywords();
    QStringList expanded;
    for (const QString &a : out)
        expanded += keywordsForAllergen(a);
    for (const QString &i : m_user.foodIntolerances)
        expanded += keywordsForIntolerance(i);
    expanded.removeDuplicates();
    return expanded;
}

QList<Recipe> RDSSEngine::filterByAllergies(const QList<Recipe> &recipes) const
{
    QStringList keys;
    for (const QString &a : m_user.allergies + User::splitLegacyText(m_user.allergens))
        keys += keywordsForAllergen(a);
    keys.removeDuplicates();
    if (keys.isEmpty())
        return recipes;

    QList<Recipe> out;
    for (const Recipe &r : recipes) {
        if (!containsAny(recipeBlob(r), keys))
            out.append(r);
    }
    return out;
}

QList<Recipe> RDSSEngine::filterByIntolerances(const QList<Recipe> &recipes) const
{
    QStringList keys;
    for (const QString &i : m_user.foodIntolerances)
        keys += keywordsForIntolerance(i);
    keys.removeDuplicates();
    if (keys.isEmpty())
        return recipes;

    QList<Recipe> out;
    for (const Recipe &r : recipes) {
        if (!containsAny(recipeBlob(r), keys))
            out.append(r);
    }
    return out;
}

QList<Recipe> RDSSEngine::filterByMedicalConditions(const QList<Recipe> &recipes) const
{
    QStringList keys;
    for (const QString &c : m_user.medicalConditions) {
        if (m_medicalBan.contains(c))
            keys += m_medicalBan.value(c);
        // 模糊匹配条件名
        for (auto it = m_medicalBan.constBegin(); it != m_medicalBan.constEnd(); ++it) {
            if (c.contains(it.key()) || it.key().contains(c))
                keys += it.value();
        }
    }
    keys.removeDuplicates();
    if (keys.isEmpty())
        return recipes;

    QList<Recipe> out;
    for (const Recipe &r : recipes) {
        if (!containsAny(recipeBlob(r), keys))
            out.append(r);
    }
    return out;
}

double RDSSEngine::deficiencyBoost(const Recipe &recipe) const
{
    if (m_user.nutritionalDeficiencies.isEmpty()
        && !m_user.medicalConditions.contains(QStringLiteral("贫血")))
        return 0.0;

    const QString blob = recipeBlob(recipe);
    double boost = 0.0;
    QStringList defs = m_user.nutritionalDeficiencies;
    if (m_user.medicalConditions.contains(QStringLiteral("贫血"))
        && !defs.contains(QStringLiteral("缺铁")))
        defs.append(QStringLiteral("缺铁"));

    for (const QString &d : defs) {
        const QString nutrient = m_deficiencyNutrient.value(d);
        if (nutrient.isEmpty())
            continue;
        const QStringList foods = m_nutrientFoodMap.value(nutrient);
        for (const QString &f : foods) {
            if (blob.contains(f, Qt::CaseInsensitive))
                boost += 12.0;
        }
        // 蛋白质不足时抬高高蛋白菜
        if (nutrient == QLatin1String("蛋白质") && recipe.totalProtein >= 20.0)
            boost += 8.0;
    }
    return boost;
}

QList<ScoredRecipe> RDSSEngine::promoteByDeficiencies(const QList<Recipe> &recipes) const
{
    QList<ScoredRecipe> out;
    out.reserve(recipes.size());
    for (const Recipe &r : recipes) {
        ScoredRecipe s;
        s.recipe = r;
        s.nutritionBoost = deficiencyBoost(r);
        s.baseScore = 0.0;
        out.append(s);
    }
    return out;
}

QList<ScoredRecipe> RDSSEngine::process(const QList<Recipe> &recipes) const
{
    QList<Recipe> filtered = filterByAllergies(recipes);
    filtered = filterByIntolerances(filtered);
    filtered = filterByMedicalConditions(filtered);

    // 饮食选择：素食者去掉明显荤类关键词
    if (m_user.dietaryChoices.contains(QStringLiteral("素食者"))
        || m_user.dietaryChoices.contains(QStringLiteral("严格素食者"))
        || m_user.dietaryChoices.contains(QStringLiteral("蛋奶素食者"))) {
        const QStringList meatKeys = {
            QStringLiteral("猪"), QStringLiteral("牛"), QStringLiteral("羊"), QStringLiteral("鸡"),
            QStringLiteral("鸭"), QStringLiteral("鱼"), QStringLiteral("虾"), QStringLiteral("蟹"),
            QStringLiteral("肉"), QStringLiteral("排骨"),
        };
        const bool allowEggMilk = m_user.dietaryChoices.contains(QStringLiteral("蛋奶素食者"));
        QList<Recipe> vegOnly;
        for (const Recipe &r : filtered) {
            const QString blob = recipeBlob(r);
            bool hit = false;
            for (const QString &k : meatKeys) {
                if (blob.contains(k)) {
                    hit = true;
                    break;
                }
            }
            if (hit && !(allowEggMilk && (blob.contains(QStringLiteral("蛋")) || blob.contains(QStringLiteral("奶")))))
                continue;
            if (m_user.dietaryChoices.contains(QStringLiteral("严格素食者"))
                && (blob.contains(QStringLiteral("蛋")) || blob.contains(QStringLiteral("奶"))))
                continue;
            vegOnly.append(r);
        }
        if (!vegOnly.isEmpty())
            filtered = vegOnly;
    }
    if (m_user.dietaryChoices.contains(QStringLiteral("避免红肉"))) {
        QList<Recipe> tmp;
        for (const Recipe &r : filtered) {
            const QString blob = recipeBlob(r);
            if (blob.contains(QStringLiteral("牛")) || blob.contains(QStringLiteral("羊"))
                || blob.contains(QStringLiteral("猪")) || blob.contains(QStringLiteral("红肉")))
                continue;
            tmp.append(r);
        }
        if (!tmp.isEmpty())
            filtered = tmp;
    }

    QList<ScoredRecipe> scored = promoteByDeficiencies(filtered);
    for (ScoredRecipe &s : scored) {
        s.baseScore = ScoreCalculator::evaluate(s.recipe, 500.0, 25.0);
    }

    std::sort(scored.begin(), scored.end(), [](const ScoredRecipe &a, const ScoredRecipe &b) {
        return a.totalScore() > b.totalScore();
    });
    return scored;
}
