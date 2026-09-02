#ifndef NUTRITIONAISERVICE_H
#define NUTRITIONAISERVICE_H

#include "../entities/Recipe.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

struct FoodVisionItem
{
    QString foodName;
    QString itemType;
    QString category;
    QString taste;
    QString commonUses;
    QString nutritionHighlights;
    double servingGrams = 0.0;
    double calories = 0.0;
    double protein = 0.0;
    double carbs = 0.0;
    double fat = 0.0;
    double confidence = 0.0;
    QString summary;
    QStringList assumptions;
    double servingMinGrams = 0.0;
    double servingMaxGrams = 0.0;
    QString calibrationBasis;
};

struct FoodVisionResult
{
    bool ok = false;
    QList<FoodVisionItem> items;
    // 聚合字段保留给已有单项界面：多目标结果时为所有 items 的合计。
    QString foodName;
    QString itemType;
    QString category;
    QString taste;
    QString commonUses;
    QString nutritionHighlights;
    double servingGrams = 0.0;
    double calories = 0.0;
    double protein = 0.0;
    double carbs = 0.0;
    double fat = 0.0;
    double confidence = 0.0;
    QString summary;
    QString answer;
    QStringList assumptions;
    double servingMinGrams = 0.0;
    double servingMaxGrams = 0.0;
    QString calibrationBasis;
    QString provider;
    QString error;
};

struct RecipeDnaIngredient
{
    QString name;
    double quantity = 0.0;
    QString quantityText;
};

struct RecipeDnaResult
{
    bool ok = false;
    QString name;
    QString category;
    QString dishRole;
    QString steps;
    int cookMinutes = 20;
    QList<RecipeDnaIngredient> ingredients;
    double calories = 0.0;
    double protein = 0.0;
    double carbs = 0.0;
    double fat = 0.0;
    QString changeSummary;
    QString provider;
    QString error;
};

Q_DECLARE_METATYPE(FoodVisionResult)
Q_DECLARE_METATYPE(RecipeDnaResult)

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class QJsonObject;
class QUrl;

class NutritionAiService : public QObject
{
    Q_OBJECT

public:
    explicit NutritionAiService(QObject *parent = nullptr);
    ~NutritionAiService() override;

    void analyzeFoodImage(const QString &imagePath);
    void analyzeFoodImages(const QStringList &imagePaths,
                           const QString &calibrationContext = {});
    void analyzeIngredientImage(const QString &imagePath);
    void describeFoodImage(const QString &imagePath);
    void describeFoodImages(const QStringList &imagePaths, const QString &question = {});
    void transformRecipe(const Recipe &recipe, const QString &instruction);
    void cancel();
    bool isBusy() const;

    static FoodVisionResult parseFoodResult(const QString &content,
                                             const QString &provider = {});
    static RecipeDnaResult parseRecipeDnaResult(const QString &content,
                                                 const QString &provider = {});
    static QString validateRecipeDnaChange(const Recipe &original,
                                           const RecipeDnaResult &result,
                                           const QString &instruction);

signals:
    void busyChanged(bool busy);
    void foodAnalysisFinished(const FoodVisionResult &result);
    void recipeTransformFinished(const RecipeDnaResult &result);

private:
    enum class Operation { None, FoodVision, IngredientVision, FoodEncyclopedia, RecipeDna };

    void analyzeImage(const QString &imagePath, Operation operation);
    void analyzeImages(const QStringList &imagePaths, Operation operation,
                       const QString &question = {});
    void startSiliconFlow();
    void startOllama();
    void post(const QUrl &url, const QJsonObject &body, const QByteArray &authorization = {});
    void handleReply();
    void finishFoodError(const QString &message);
    void finishDnaError(const QString &message);
    void finishRequest();
    void stopRequest(bool notifyBusyChange);
    QString foodPrompt() const;
    QString ingredientPrompt() const;
    QString encyclopediaPrompt() const;
    QString imagePrompt() const;
    QString dnaPrompt() const;

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    QTimer *m_timeout = nullptr;
    Operation m_operation = Operation::None;
    bool m_triedOllama = false;
    int m_cloudRetryCount = 0;
    bool m_cancelled = false;
    QList<QByteArray> m_imageBase64List;
    QString m_userQuestion;
    QString m_calibrationContext;
    Recipe m_recipe;
    QString m_instruction;
    QString m_lastProvider;
    QString m_lastNetworkError;
    QString m_dnaValidationError;
    bool m_dnaRepairAttempted = false;
};

#endif // NUTRITIONAISERVICE_H
