#ifndef RECOMMENDRESULT_H
#define RECOMMENDRESULT_H

#include "Recipe.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>

struct MealSlot
{
    QString mealLabel; // 早餐 / 午餐 / 晚餐
    QList<Recipe> dishes;

    bool isValid() const { return !dishes.isEmpty(); }

    int roleCount(const QString &role) const
    {
        int count = 0;
        for (const Recipe &dish : dishes)
            count += dish.dishRole == role;
        return count;
    }

    bool hasExactlyOneStaple() const { return roleCount(QStringLiteral("staple")) == 1; }

    bool hasBalancedMainMeal() const
    {
        return roleCount(QStringLiteral("meat")) >= 1
               && roleCount(QStringLiteral("vegetable")) >= 1
               && hasExactlyOneStaple();
    }

    QString title() const
    {
        QStringList names;
        names.reserve(dishes.size());
        for (const Recipe &d : dishes)
            names.append(d.name);
        return names.join(QStringLiteral(" + "));
    }

    double totalCalories() const
    {
        double sum = 0.0;
        for (const Recipe &d : dishes)
            sum += d.totalCalories;
        return sum;
    }

    double totalProtein() const
    {
        double sum = 0.0;
        for (const Recipe &d : dishes)
            sum += d.totalProtein;
        return sum;
    }

    double totalCarbs() const
    {
        double sum = 0.0;
        for (const Recipe &d : dishes)
            sum += d.totalCarbs;
        return sum;
    }

    double totalFat() const
    {
        double sum = 0.0;
        for (const Recipe &d : dishes)
            sum += d.totalFat;
        return sum;
    }

    int cookMinutes() const
    {
        int m = 0;
        for (const Recipe &d : dishes)
            m = qMax(m, d.cookMinutes);
        return m > 0 ? m : 20;
    }

    Recipe primary() const { return dishes.isEmpty() ? Recipe{} : dishes.first(); }

    QList<int> ids() const
    {
        QList<int> out;
        for (const Recipe &d : dishes)
            out.append(d.id);
        return out;
    }
};

struct RecommendResult
{
    MealSlot breakfast;
    MealSlot lunch;
    MealSlot dinner;
    QString summary;
    QStringList reasons; // 推荐理由（知识库生成）
    bool valid = false;
};

#endif // RECOMMENDRESULT_H
