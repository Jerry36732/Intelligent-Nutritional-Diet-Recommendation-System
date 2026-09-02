#include "../src/services/AiAssistantService.h"

#include <QCoreApplication>
#include <QDebug>

#include <cstdio>

namespace {
int failures = 0;

void expect(bool condition, const char *message)
{
    if (condition)
        qInfo() << "OK  :" << message;
    else {
        qCritical() << "FAIL:" << message;
        ++failures;
    }
}
}

int main(int argc, char *argv[])
{
    std::fprintf(stderr, "ai-assistant: application\n");
    std::fflush(stderr);
    QCoreApplication app(argc, argv);
    User user;
    user.id = 1;
    user.name = QStringLiteral("测试用户");
    user.preferences = QStringLiteral("清淡");

    AiAssistantService service;
    AiPreferenceUpdate captured;
    bool received = false;
    QObject::connect(&service, &AiAssistantService::finished,
                     [&](const AiPreferenceUpdate &result) {
        captured = result;
        received = true;
    });

    std::fprintf(stderr, "ai-assistant: analyze\n");
    std::fflush(stderr);
    service.analyzeUserMessage(user, QStringLiteral("我不喜欢豆腐"));
    std::fprintf(stderr, "ai-assistant: analyzed received=%d\n", received ? 1 : 0);
    std::fflush(stderr);

    expect(received, "明确负向偏好由本地规则立即处理");
    expect(captured.ok, "本地偏好处理成功");
    expect(captured.provider == QStringLiteral("local-rules"), "未调用外部模型");
    expect(captured.preferences.contains(QStringLiteral("不吃豆腐")), "偏好写入不吃豆腐");
    expect(captured.allergens.isEmpty(), "不喜欢不被误写为过敏原");
    expect(captured.regenerate, "偏好变化触发重新生成方案");
    expect(captured.reply.contains(QStringLiteral("避开"))
               && captured.reply.contains(QStringLiteral("豆腐")),
           "回复明确确认避开豆腐");

    std::fprintf(stderr, "ai-assistant: complete failures=%d\n", failures);
    std::fflush(stderr);
    return failures == 0 ? 0 : 1;
}
