#ifndef USER_H
#define USER_H

#include <QJsonObject>
#include <QString>
#include <QStringList>

struct User
{
    int id = 0;
    QString name;
    QString gender;          // "male" / "female"
    QString goal;            // "lose" / "gain" / "maintain"
    double height = 0.0;     // cm
    double weight = 0.0;     // kg
    int calorieTarget = 0;   // kcal/day
    QString passwordHash;    // "salt:sha256hex" — never log plaintext
    QString preferences;     // 口味偏好（兼容旧字段，逗号/顿号分隔）
    QString allergens;       // 过敏原文本（兼容旧字段，与 allergies 同步）

    // 多维健康档案（论文用户模型）— 存库为 JSON 数组
    QStringList dietaryChoices;           // 饮食选择
    QStringList foodIntolerances;         // 食物不耐受
    QStringList nutritionalDeficiencies;  // 营养缺乏
    QStringList allergies;                // 过敏史
    QStringList medicalConditions;        // 医疗状况

    bool isValid() const { return id > 0 || !name.isEmpty(); }

    /** 将 allergies 与旧字段 allergens 双向对齐 */
    void syncAllergenFields();
    /** 推荐引擎使用的规避关键词：过敏 + 不耐受 */
    QStringList avoidanceKeywords() const;

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);

    static QString stringListToJson(const QStringList &list);
    static QStringList stringListFromJson(const QString &json);
    static QStringList splitLegacyText(const QString &text);
    static QString joinLegacyText(const QStringList &list);
};

#endif // USER_H
