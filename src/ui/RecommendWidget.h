#ifndef RECOMMENDWIDGET_H
#define RECOMMENDWIDGET_H

#include "../entities/RecommendResult.h"
#include "../entities/User.h"
#include "../services/AiAssistantService.h"
#include "../services/NutritionAiService.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QFrame;
class QScrollArea;
class QBoxLayout;
class QHBoxLayout;
class QVBoxLayout;
class RecipeCard;

class RecommendWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RecommendWidget(QWidget *parent = nullptr);

    void setUser(const User &user);
    void setPlan(const RecommendResult &plan);
    QString selectedGoalCode() const;
    void setAiPanelVisible(bool visible);
    bool isAiPanelVisible() const { return m_aiVisible; }
    void toggleAiPanel();
    void setPhotoReviewState();

signals:
    void generateRequested();
    void aiPreferenceApplied(const AiPreferenceUpdate &update);
    void detailRequested(const Recipe &recipe);
    void mealDetailRequested(const MealSlot &meal);
    void favoriteToggled(int recipeId);
    void aiPanelVisibilityChanged(bool visible);
    void todayPlanRequested();

private slots:
    void onSendChat();
    void onAiFinished(const AiPreferenceUpdate &result);
    void onSuggestionClicked();
    void onAnalyzePhoto();
    void onImageAnalysisFinished(const FoodVisionResult &result);

private:
    void refreshMeta();
    void setChatBusy(bool busy);
    void rebuildMealLayout(bool wide);
    void addChatBubble(const QString &text, bool fromUser);
    void addPlanReadyBubble();
    void updateAttachmentTray();
    QStringList detectAllergyMentions(const QString &message) const;

    User m_user;
    RecommendResult m_plan;
    AiAssistantService *m_ai = nullptr;
    NutritionAiService *m_imageAi = nullptr;
    bool m_aiVisible = true;
    bool m_chatBusy = false;
    QString m_lastUserMessage;
    QStringList m_selectedImagePaths;
    QStringList m_activeImagePaths;
    QString m_activeImageQuestion;
    QString m_activeImageUserText;

    QLabel *m_metaLabel = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QLabel *m_reasonLabel = nullptr;
    QLabel *m_providerLabel = nullptr;
    QPushButton *m_generateBtn = nullptr;
    QPushButton *m_toggleAiBtn = nullptr;
    RecipeCard *m_breakfastCard = nullptr;
    RecipeCard *m_lunchCard = nullptr;
    RecipeCard *m_dinnerCard = nullptr;

    QFrame *m_planPanel = nullptr;
    QFrame *m_aiPanel = nullptr;
    QWidget *m_mealHost = nullptr;
    QBoxLayout *m_mealLay = nullptr;
    QHBoxLayout *m_split = nullptr;

    QScrollArea *m_chatScroll = nullptr;
    QWidget *m_chatHost = nullptr;
    QVBoxLayout *m_chatLay = nullptr;
    QLineEdit *m_chatInput = nullptr;
    QPushButton *m_sendBtn = nullptr;
    QPushButton *m_photoBtn = nullptr;
    QFrame *m_attachmentTray = nullptr;
    QLabel *m_attachmentLabel = nullptr;
    QPushButton *m_clearAttachmentsBtn = nullptr;
};

#endif // RECOMMENDWIDGET_H
