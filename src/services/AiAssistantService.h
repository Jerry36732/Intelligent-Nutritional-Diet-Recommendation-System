#ifndef AIASSISTANTSERVICE_H
#define AIASSISTANTSERVICE_H

#include "../entities/RecommendResult.h"
#include "../entities/User.h"

#include <QObject>
#include <QString>

struct AiPreferenceUpdate
{
    QString reply;
    QString goal;          // lose / gain / maintain，空表示不变
    QString preferences;   // 空表示不变
    QString allergens;     // 空表示不变
    bool regenerate = false;
    bool ok = false;
    QString error;
    QString provider;      // siliconflow / ollama
};

class AiAssistantService : public QObject
{
    Q_OBJECT

public:
    explicit AiAssistantService(QObject *parent = nullptr);

    void analyzeUserMessage(const User &user,
                            const QString &userMessage,
                            const RecommendResult &currentPlan = RecommendResult{});

signals:
    void finished(const AiPreferenceUpdate &result);

private:
    void trySiliconFlow(const User &user, const QString &userMessage);
    void tryOllama(const User &user, const QString &userMessage);
    void finishWithError(const QString &error);
    AiPreferenceUpdate parseModelOutput(const QString &content, const QString &provider) const;
    QString buildSystemPrompt(const User &user) const;
    QString planContextText() const;
    QString loadApiKey() const;
    QString siliconModel() const;
    QString ollamaModel() const;

    User m_pendingUser;
    QString m_pendingMessage;
    RecommendResult m_currentPlan;
};

#endif // AIASSISTANTSERVICE_H
