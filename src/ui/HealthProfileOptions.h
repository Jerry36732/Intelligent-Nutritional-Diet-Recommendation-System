#ifndef HEALTHPROFILEOPTIONS_H
#define HEALTHPROFILEOPTIONS_H

#include <QStringList>

/** 注册 / 设置页共用的健康档案选项 */
namespace HealthProfileOptions {

inline QStringList dietaryChoices()
{
    return {
        QStringLiteral("荤食者"),
        QStringLiteral("素食者"),
        QStringLiteral("蛋奶素食者"),
        QStringLiteral("严格素食者"),
        QStringLiteral("清真"),
        QStringLiteral("避免红肉"),
        QStringLiteral("低脂"),
        QStringLiteral("低盐"),
        QStringLiteral("低糖"),
        QStringLiteral("高蛋白"),
        QStringLiteral("少油少辣"),
        QStringLiteral("清淡"),
        QStringLiteral("少碳水"),
        QStringLiteral("地中海饮食"),
        QStringLiteral("轻断食友好"),
    };
}

inline QStringList allergies()
{
    return {
        QStringLiteral("花生"),
        QStringLiteral("坚果"),
        QStringLiteral("鸡蛋"),
        QStringLiteral("牛奶"),
        QStringLiteral("海鲜"),
        QStringLiteral("贝类"),
        QStringLiteral("大豆"),
        QStringLiteral("豆制品"),
        QStringLiteral("麸质（小麦）"),
        QStringLiteral("芝麻"),
        QStringLiteral("猕猴桃"),
        QStringLiteral("芒果"),
        QStringLiteral("桃子"),
        QStringLiteral("草莓"),
        QStringLiteral("菠萝"),
        QStringLiteral("番茄"),
        QStringLiteral("芹菜"),
        QStringLiteral("胡萝卜"),
        QStringLiteral("大蒜"),
        QStringLiteral("洋葱"),
        QStringLiteral("芥末"),
        QStringLiteral("胡椒"),
        QStringLiteral("巧克力"),
        QStringLiteral("酵母"),
        QStringLiteral("虾"),
        QStringLiteral("蟹"),
        QStringLiteral("鳕鱼"),
        QStringLiteral("三文鱼"),
        QStringLiteral("酒精"),
    };
}

inline QStringList foodIntolerances()
{
    return {
        QStringLiteral("乳糖"),
        QStringLiteral("麸质"),
        QStringLiteral("果糖"),
        QStringLiteral("水杨酸盐"),
        QStringLiteral("亚硫酸盐"),
        QStringLiteral("FODMAPs"),
        QStringLiteral("组胺"),
        QStringLiteral("咖啡因"),
    };
}

inline QStringList nutritionalDeficiencies()
{
    return {
        QStringLiteral("缺铁"),
        QStringLiteral("缺钙"),
        QStringLiteral("缺维生素D"),
        QStringLiteral("缺维生素B12"),
        QStringLiteral("蛋白质不足"),
        QStringLiteral("缺锌"),
        QStringLiteral("缺镁"),
        QStringLiteral("缺叶酸"),
    };
}

inline QStringList medicalConditions()
{
    return {
        QStringLiteral("2型糖尿病"),
        QStringLiteral("高血压"),
        QStringLiteral("高血脂"),
        QStringLiteral("心血管疾病"),
        QStringLiteral("肥胖"),
        QStringLiteral("贫血"),
        QStringLiteral("肾病"),
        QStringLiteral("痛风"),
        QStringLiteral("脂肪肝"),
        QStringLiteral("胃食管反流"),
    };
}

} // namespace HealthProfileOptions

#endif // HEALTHPROFILEOPTIONS_H
