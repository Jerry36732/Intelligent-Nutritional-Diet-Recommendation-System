#include "KnowledgeBase.h"
#include "../dao/DatabaseManager.h"

#include <QHash>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QVariant>

void KnowledgeBase::ensureLoaded() const
{
    if (!m_loaded)
        const_cast<KnowledgeBase *>(this)->loadCache();
}

void KnowledgeBase::reload()
{
    m_loaded = false;
    m_nutrientToFoodIds.clear();
    m_allergenToFoodIds.clear();
    m_deficiencyToFoodIds.clear();
    m_diseaseBanKeywords.clear();
    m_foodById.clear();
    loadCache();
}

void KnowledgeBase::loadCache()
{
    m_nutrientToFoodIds.clear();
    m_allergenToFoodIds.clear();
    m_deficiencyToFoodIds.clear();
    m_diseaseBanKeywords.clear();
    m_foodById.clear();

    if (!DatabaseManager::getInstance().isOpen()) {
        m_loaded = true;
        return;
    }
    QSqlDatabase db = DatabaseManager::getInstance().database();
    QSqlQuery q(db);

    if (q.exec(QStringLiteral(
            "SELECT id, name, IFNULL(category_label,''), calories, protein, carbs, fat, unit "
            "FROM foods"))) {
        while (q.next()) {
            Food f;
            f.id = q.value(0).toInt();
            f.name = q.value(1).toString();
            f.category = q.value(2).toString();
            f.calories = q.value(3).toDouble();
            f.protein = q.value(4).toDouble();
            f.carbs = q.value(5).toDouble();
            f.fat = q.value(6).toDouble();
            f.unit = q.value(7).toString();
            m_foodById.insert(f.id, f);
        }
    }

    if (q.exec(QStringLiteral(
            "SELECT nutrient_id, food_id FROM food_nutrient_relations "
            "WHERE concentration_level IN ('高','中')"))) {
        while (q.next())
            m_nutrientToFoodIds[q.value(0).toString()].append(q.value(1).toInt());
    }

    if (q.exec(QStringLiteral("SELECT allergen_name, food_id FROM allergen_food_mapping"))) {
        while (q.next())
            m_allergenToFoodIds[q.value(0).toString()].append(q.value(1).toInt());
    }

    if (q.exec(QStringLiteral(
            "SELECT deficiency_name, food_id FROM nutrient_deficiency_foods ORDER BY priority"))) {
        while (q.next())
            m_deficiencyToFoodIds[q.value(0).toString()].append(q.value(1).toInt());
    }

    if (q.exec(QStringLiteral(
            "SELECT disease_name, description FROM disease_nutrient_rules WHERE rule_type='限制'"))) {
        while (q.next()) {
            const QString disease = q.value(0).toString();
            const QString desc = q.value(1).toString();
            // description 中可放关键词，逗号分隔
            for (QString part : desc.split(QRegularExpression(QStringLiteral("[,，、]")),
                                           Qt::SkipEmptyParts)) {
                part = part.trimmed();
                if (!part.isEmpty())
                    m_diseaseBanKeywords[disease].append(part);
            }
        }
    }

    // 内置兜底关键词（库空时）
    if (m_diseaseBanKeywords.isEmpty()) {
        m_diseaseBanKeywords[QStringLiteral("2型糖尿病")] =
            {QStringLiteral("甜品"), QStringLiteral("蛋糕"), QStringLiteral("糖水")};
        m_diseaseBanKeywords[QStringLiteral("高血压")] =
            {QStringLiteral("腌"), QStringLiteral("咸鱼"), QStringLiteral("腊肉")};
        m_diseaseBanKeywords[QStringLiteral("高血脂")] =
            {QStringLiteral("肥肉"), QStringLiteral("油炸")};
    }

    m_loaded = true;
}

QList<Food> KnowledgeBase::getFoodsRichInNutrient(const QString &nutrientName) const
{
    ensureLoaded();
    QList<Food> out;
    for (int id : m_nutrientToFoodIds.value(nutrientName)) {
        if (m_foodById.contains(id))
            out.append(m_foodById.value(id));
    }
    return out;
}

QList<Food> KnowledgeBase::getFoodsToAvoidForAllergy(const QString &allergen) const
{
    ensureLoaded();
    QList<Food> out;
    QStringList keys = m_allergenToFoodIds.keys();
    for (const QString &k : keys) {
        if (k.contains(allergen) || allergen.contains(k)) {
            for (int id : m_allergenToFoodIds.value(k)) {
                if (m_foodById.contains(id))
                    out.append(m_foodById.value(id));
            }
        }
    }
    return out;
}

QList<Food> KnowledgeBase::getFoodsToAvoidForDisease(const QString &disease) const
{
    ensureLoaded();
    QList<Food> out;
    const QStringList kws = m_diseaseBanKeywords.value(disease);
    for (auto it = m_foodById.constBegin(); it != m_foodById.constEnd(); ++it) {
        for (const QString &kw : kws) {
            if (it.value().name.contains(kw)) {
                out.append(it.value());
                break;
            }
        }
    }
    return out;
}

QStringList KnowledgeBase::foodNamesRichIn(const QString &nutrientName, int limit) const
{
    QStringList names;
    const QList<Food> foods = getFoodsRichInNutrient(nutrientName);
    for (const Food &f : foods) {
        names.append(f.name);
        if (names.size() >= limit)
            break;
    }
    return names;
}

QStringList KnowledgeBase::avoidKeywordsForAllergy(const QString &allergen) const
{
    QStringList names;
    for (const Food &f : getFoodsToAvoidForAllergy(allergen))
        names.append(f.name);
    names.removeDuplicates();
    return names;
}

QStringList KnowledgeBase::getDietaryGuidelines(const User &user) const
{
    ensureLoaded();
    QStringList tips;
    for (const QString &d : user.nutritionalDeficiencies) {
        const QStringList foods = [&]() {
            QStringList n;
            for (int id : m_deficiencyToFoodIds.value(d)) {
                if (m_foodById.contains(id))
                    n.append(m_foodById.value(id).name);
                if (n.size() >= 4)
                    break;
            }
            return n;
        }();
        if (!foods.isEmpty())
            tips.append(QStringLiteral("针对「%1」：优先选择 %2。").arg(d, foods.join(QStringLiteral("、"))));
        else if (d.contains(QStringLiteral("铁")))
            tips.append(QStringLiteral("针对「%1」：可增加菠菜、瘦红肉、动物肝脏等富铁食物。").arg(d));
        else if (d.contains(QStringLiteral("钙")))
            tips.append(QStringLiteral("针对「%1」：可增加奶制品、豆制品、芝麻等富钙食物。").arg(d));
        else
            tips.append(QStringLiteral("针对「%1」：请在三餐中均衡补充相关营养素。").arg(d));
    }
    for (const QString &a : user.allergies + User::splitLegacyText(user.allergens))
        tips.append(QStringLiteral("过敏原「%1」：已启用硬过滤，相关食材不会进入推荐。").arg(a));
    for (const QString &c : user.medicalConditions) {
        const QStringList ban = m_diseaseBanKeywords.value(c);
        if (!ban.isEmpty())
            tips.append(QStringLiteral("医疗状况「%1」：限制 %2 等食物。")
                            .arg(c, ban.mid(0, 3).join(QStringLiteral("、"))));
    }
    if (tips.isEmpty())
        tips.append(QStringLiteral("保持三餐热量 30%/40%/30% 分配，并保证每日蛋白质充足。"));
    return tips;
}

QStringList KnowledgeBase::buildRecommendationReasons(const User &user,
                                                      const RecommendResult &plan) const
{
    ensureLoaded();
    QStringList reasons;
    if (!plan.valid)
        return reasons;

    QStringList dishNames;
    auto collect = [&](const MealSlot &slot) {
        for (const Recipe &r : slot.dishes)
            dishNames.append(r.name);
    };
    collect(plan.breakfast);
    collect(plan.lunch);
    collect(plan.dinner);
    const QString allText = dishNames.join(QLatin1Char(' '));

    auto nutrientForDef = [](const QString &d) -> QString {
        if (d.contains(QStringLiteral("铁")) || d.contains(QStringLiteral("贫血")))
            return QStringLiteral("铁");
        if (d.contains(QStringLiteral("钙")))
            return QStringLiteral("钙");
        if (d.contains(QStringLiteral("维生素D")))
            return QStringLiteral("维生素D");
        if (d.contains(QStringLiteral("B12")))
            return QStringLiteral("维生素B12");
        if (d.contains(QStringLiteral("蛋白")))
            return QStringLiteral("蛋白质");
        return {};
    };

    for (const QString &def : user.nutritionalDeficiencies) {
        const QString nutrient = nutrientForDef(def);
        if (nutrient.isEmpty())
            continue;
        const QStringList rich = foodNamesRichIn(nutrient, 12);
        for (const QString &foodName : rich) {
            if (allText.contains(foodName) || dishNames.join(QString()).contains(foodName.left(2))) {
                reasons.append(
                    QStringLiteral("本方案含「%1」相关食材，因其富含%2，有助于改善您的「%3」。")
                        .arg(foodName, nutrient, def));
                break;
            }
        }
        // 菜名启发式
        if (reasons.isEmpty() || !reasons.last().contains(def)) {
            if (nutrient == QLatin1String("铁") && allText.contains(QStringLiteral("菠菜")))
                reasons.append(QStringLiteral("本方案推荐菠菜，因为它富含铁元素，有助于改善您的缺铁状况。"));
            else if (nutrient == QLatin1String("钙")
                     && (allText.contains(QStringLiteral("奶")) || allText.contains(QStringLiteral("豆腐"))))
                reasons.append(QStringLiteral("本方案含乳制品/豆制品，有助于补充钙，改善「%1」。").arg(def));
            else if (nutrient == QLatin1String("蛋白质")
                     && (allText.contains(QStringLiteral("鸡")) || allText.contains(QStringLiteral("蛋"))
                         || allText.contains(QStringLiteral("鱼")) || allText.contains(QStringLiteral("肉"))))
                reasons.append(QStringLiteral("本方案含优质蛋白食材，有助于改善「%1」。").arg(def));
        }
    }

    if (!user.allergies.isEmpty() || !user.allergens.isEmpty()) {
        reasons.append(QStringLiteral("已根据您的过敏史做硬过滤，方案中避开相关食材。"));
    }
    if (!user.medicalConditions.isEmpty()) {
        reasons.append(QStringLiteral("已结合医疗状况（%1）限制高风险食物。")
                           .arg(user.medicalConditions.join(QStringLiteral("、"))));
    }
    if (reasons.isEmpty()) {
        reasons.append(QStringLiteral("本方案按日热量 30%% / 40%% / 30%% 分配，并尽量贴近您的目标「%1」。")
                           .arg(user.goal));
    }
    reasons.removeDuplicates();
    return reasons;
}
