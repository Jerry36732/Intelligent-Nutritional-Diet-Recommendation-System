#include "RecommendWidget.h"
#include "RecipeCard.h"

#include "../services/UserService.h"

#include <QBoxLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

RecommendWidget::RecommendWidget(QWidget *parent)
    : QWidget(parent)
    , m_ai(new AiAssistantService(this))
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 16);
    root->setSpacing(12);

    auto *titleRow = new QHBoxLayout;
    auto *titleBox = new QVBoxLayout;
    titleBox->setSpacing(4);
    auto *title = new QLabel(QStringLiteral("智能推荐"), this);
    title->setProperty("class", QVariant(QStringLiteral("SectionTitle")));
    auto *intro = new QLabel(
        QStringLiteral("左侧为今日三餐；右侧 AI 可问答与改忌口。点「隐藏助手」或侧栏「膳衡」Logo 可收起助手，方案区更宽。"),
        this);
    intro->setWordWrap(true);
    intro->setProperty("class", QVariant(QStringLiteral("HintText")));
    titleBox->addWidget(title);
    titleBox->addWidget(intro);
    titleRow->addLayout(titleBox, 1);

    m_toggleAiBtn = new QPushButton(QStringLiteral("隐藏助手"), this);
    m_toggleAiBtn->setCursor(Qt::PointingHandCursor);
    m_toggleAiBtn->setProperty("class", QVariant(QStringLiteral("GhostButton")));
    m_toggleAiBtn->setToolTip(QStringLiteral("隐藏/显示 AI 助手；也可点击侧栏 Logo"));
    titleRow->addWidget(m_toggleAiBtn, 0, Qt::AlignTop);

    m_split = new QHBoxLayout;
    m_split->setSpacing(14);

    // ---- 左侧：三餐方案 ----
    m_planPanel = new QFrame(this);
    m_planPanel->setObjectName(QStringLiteral("RecommendPlanPanel"));
    m_planPanel->setProperty("class", QVariant(QStringLiteral("PanelCard")));
    auto *leftLay = new QVBoxLayout(m_planPanel);
    leftLay->setContentsMargins(18, 16, 18, 16);
    leftLay->setSpacing(12);

    auto *leftHeader = new QHBoxLayout;
    auto *leftTitle = new QLabel(QStringLiteral("今日三餐方案"), m_planPanel);
    leftTitle->setProperty("class", QVariant(QStringLiteral("SectionTitle")));
    m_generateBtn = new QPushButton(QStringLiteral("重新生成"), m_planPanel);
    m_generateBtn->setCursor(Qt::PointingHandCursor);
    m_generateBtn->setProperty("class", QVariant(QStringLiteral("CoralButton")));
    leftHeader->addWidget(leftTitle);
    leftHeader->addStretch();
    leftHeader->addWidget(m_generateBtn);

    m_metaLabel = new QLabel(m_planPanel);
    m_metaLabel->setWordWrap(true);
    m_metaLabel->setProperty("class", QVariant(QStringLiteral("HintText")));

    m_summaryLabel = new QLabel(m_planPanel);
    m_summaryLabel->setObjectName(QStringLiteral("RecommendSummary"));
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setProperty("class", QVariant(QStringLiteral("HintText")));

    m_reasonLabel = new QLabel(m_planPanel);
    m_reasonLabel->setObjectName(QStringLiteral("RecommendReasons"));
    m_reasonLabel->setWordWrap(true);
    m_reasonLabel->setProperty("class", QVariant(QStringLiteral("HintText")));
    m_reasonLabel->setText(QStringLiteral("推荐理由将在生成方案后显示。"));

    auto *mealScroll = new QScrollArea(m_planPanel);
    mealScroll->setWidgetResizable(true);
    mealScroll->setFrameShape(QFrame::NoFrame);
    mealScroll->setObjectName(QStringLiteral("RecommendMealScroll"));
    m_mealHost = new QWidget;
    m_mealLay = new QBoxLayout(QBoxLayout::TopToBottom, m_mealHost);
    m_mealLay->setContentsMargins(0, 0, 0, 0);
    m_mealLay->setSpacing(12);

    m_breakfastCard = new RecipeCard(m_mealHost);
    m_lunchCard = new RecipeCard(m_mealHost);
    m_dinnerCard = new RecipeCard(m_mealHost);
    m_breakfastCard->setMinimumHeight(0);
    m_lunchCard->setMinimumHeight(0);
    m_dinnerCard->setMinimumHeight(0);

    m_mealLay->addWidget(m_breakfastCard, 1);
    m_mealLay->addWidget(m_lunchCard, 1);
    m_mealLay->addWidget(m_dinnerCard, 1);
    mealScroll->setWidget(m_mealHost);

    leftLay->addLayout(leftHeader);
    leftLay->addWidget(m_metaLabel);
    leftLay->addWidget(m_summaryLabel);
    leftLay->addWidget(m_reasonLabel);
    leftLay->addWidget(mealScroll, 1);

    // ---- 右侧：AI 助手（可隐藏）----
    m_aiPanel = new QFrame(this);
    m_aiPanel->setObjectName(QStringLiteral("AiAssistantPanel"));
    m_aiPanel->setProperty("class", QVariant(QStringLiteral("PanelCard")));
    m_aiPanel->setMinimumWidth(300);
    m_aiPanel->setMaximumWidth(380);
    auto *rightLay = new QVBoxLayout(m_aiPanel);
    rightLay->setContentsMargins(14, 14, 14, 12);
    rightLay->setSpacing(10);

    auto *aiHeaderRow = new QHBoxLayout;
    auto *aiTitleBox = new QVBoxLayout;
    aiTitleBox->setSpacing(2);
    auto *rightTitle = new QLabel(QStringLiteral("AI 营养助手"), m_aiPanel);
    rightTitle->setProperty("class", QVariant(QStringLiteral("SectionTitle")));
    m_providerLabel = new QLabel(QStringLiteral("硅基流动 / 本地 Ollama"), m_aiPanel);
    m_providerLabel->setProperty("class", QVariant(QStringLiteral("HintText")));
    aiTitleBox->addWidget(rightTitle);
    aiTitleBox->addWidget(m_providerLabel);
    auto *closeAiBtn = new QPushButton(QStringLiteral("收起"), m_aiPanel);
    closeAiBtn->setCursor(Qt::PointingHandCursor);
    closeAiBtn->setProperty("class", QVariant(QStringLiteral("GhostButton")));
    closeAiBtn->setFixedWidth(56);
    aiHeaderRow->addLayout(aiTitleBox, 1);
    aiHeaderRow->addWidget(closeAiBtn, 0, Qt::AlignTop);

    m_chatScroll = new QScrollArea(m_aiPanel);
    m_chatScroll->setWidgetResizable(true);
    m_chatScroll->setFrameShape(QFrame::NoFrame);
    m_chatScroll->setObjectName(QStringLiteral("AiChatScroll"));
    m_chatHost = new QWidget;
    m_chatHost->setObjectName(QStringLiteral("AiChatHost"));
    m_chatLay = new QVBoxLayout(m_chatHost);
    m_chatLay->setContentsMargins(8, 8, 8, 8);
    m_chatLay->setSpacing(10);
    m_chatLay->addStretch();
    m_chatScroll->setWidget(m_chatHost);

    auto *suggestWrap = new QWidget(m_aiPanel);
    auto *suggestLay = new QVBoxLayout(suggestWrap);
    suggestLay->setContentsMargins(0, 0, 0, 0);
    suggestLay->setSpacing(6);
    auto *suggestHint = new QLabel(QStringLiteral("试试这些："), suggestWrap);
    suggestHint->setProperty("class", QVariant(QStringLiteral("HintText")));
    suggestLay->addWidget(suggestHint);
    auto *suggestRow = new QHBoxLayout;
    suggestRow->setSpacing(6);
    const QStringList suggestions = {
        QStringLiteral("海蜇皮算海鲜吗"),
        QStringLiteral("忌口花生牛奶"),
        QStringLiteral("午餐换清淡"),
    };
    for (const QString &s : suggestions) {
        auto *chip = new QPushButton(s, suggestWrap);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setProperty("class", QVariant(QStringLiteral("SuggestChip")));
        chip->setFlat(true);
        connect(chip, &QPushButton::clicked, this, &RecommendWidget::onSuggestionClicked);
        suggestRow->addWidget(chip);
    }
    suggestRow->addStretch();
    suggestLay->addLayout(suggestRow);

    auto *inputWrap = new QFrame(m_aiPanel);
    inputWrap->setObjectName(QStringLiteral("AiInputBar"));
    auto *inputLay = new QHBoxLayout(inputWrap);
    inputLay->setContentsMargins(10, 8, 8, 8);
    inputLay->setSpacing(8);
    m_chatInput = new QLineEdit(inputWrap);
    m_chatInput->setObjectName(QStringLiteral("AiChatInput"));
    m_chatInput->setPlaceholderText(QStringLiteral("提问，或说忌口 / 偏好…"));
    m_chatInput->setClearButtonEnabled(true);
    m_sendBtn = new QPushButton(QStringLiteral("发送"), inputWrap);
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    m_sendBtn->setProperty("class", QVariant(QStringLiteral("PrimaryButton")));
    m_sendBtn->setFixedWidth(72);
    inputLay->addWidget(m_chatInput, 1);
    inputLay->addWidget(m_sendBtn);

    rightLay->addLayout(aiHeaderRow);
    rightLay->addWidget(m_chatScroll, 1);
    rightLay->addWidget(suggestWrap);
    rightLay->addWidget(inputWrap);

    m_split->addWidget(m_planPanel, 1);
    m_split->addWidget(m_aiPanel, 0);

    root->addLayout(titleRow);
    root->addLayout(m_split, 1);

    connect(m_generateBtn, &QPushButton::clicked, this, &RecommendWidget::generateRequested);
    connect(m_toggleAiBtn, &QPushButton::clicked, this, &RecommendWidget::toggleAiPanel);
    connect(closeAiBtn, &QPushButton::clicked, this, &RecommendWidget::toggleAiPanel);
    connect(m_sendBtn, &QPushButton::clicked, this, &RecommendWidget::onSendChat);
    connect(m_chatInput, &QLineEdit::returnPressed, this, &RecommendWidget::onSendChat);
    connect(m_ai, &AiAssistantService::finished, this, &RecommendWidget::onAiFinished);

    for (RecipeCard *card : {m_breakfastCard, m_lunchCard, m_dinnerCard}) {
        connect(card, &RecipeCard::detailClicked, this, &RecommendWidget::detailRequested);
        connect(card, &RecipeCard::mealDetailRequested, this, &RecommendWidget::mealDetailRequested);
        connect(card, &RecipeCard::favoriteToggled, this, &RecommendWidget::favoriteToggled);
    }

    m_breakfastCard->clear();
    m_lunchCard->clear();
    m_dinnerCard->clear();
    m_summaryLabel->setText(QStringLiteral("尚未生成方案。可点「重新生成」，或打开 AI 助手说明忌口/偏好。"));

    addChatBubble(
        QStringLiteral("你好，我是膳衡 AI。可以问营养问题，也可以说忌口与喜好。需要更大方案区时，点「收起」或侧栏 Logo。"),
        false);
}

void RecommendWidget::setUser(const User &user)
{
    m_user = user;
    refreshMeta();
}

void RecommendWidget::setPlan(const RecommendResult &plan)
{
    m_plan = plan;
    if (plan.valid) {
        m_breakfastCard->setMeal(plan.breakfast);
        m_lunchCard->setMeal(plan.lunch);
        m_dinnerCard->setMeal(plan.dinner);
        m_summaryLabel->setText(plan.summary);
        if (!plan.reasons.isEmpty()) {
            QStringList lines;
            for (const QString &r : plan.reasons)
                lines.append(QStringLiteral("· ") + r);
            m_reasonLabel->setText(QStringLiteral("推荐理由\n") + lines.join(QLatin1Char('\n')));
        } else {
            m_reasonLabel->setText(QStringLiteral("推荐理由：已按热量目标与健康档案生成。"));
        }
    } else {
        m_breakfastCard->clear();
        m_lunchCard->clear();
        m_dinnerCard->clear();
        m_summaryLabel->setText(plan.summary.isEmpty()
                                    ? QStringLiteral("尚未生成方案，可点「重新生成」。")
                                    : plan.summary);
        m_reasonLabel->setText(QStringLiteral("推荐理由将在生成方案后显示。"));
    }
    refreshMeta();
}

QString RecommendWidget::selectedGoalCode() const
{
    return m_user.goal.isEmpty() ? QStringLiteral("maintain") : m_user.goal;
}

void RecommendWidget::refreshMeta()
{
    QString goalCn = QStringLiteral("维持");
    const QString g = m_user.goal.toLower();
    if (g == QLatin1String("lose"))
        goalCn = QStringLiteral("减重");
    else if (g == QLatin1String("gain"))
        goalCn = QStringLiteral("增肌");

    UserService svc;
    const int cal = m_user.calorieTarget > 0 ? m_user.calorieTarget : svc.calculateDailyCalories(m_user);
    m_metaLabel->setText(
        QStringLiteral("目标 %1 · %2 kcal/日　　偏好：%3　　忌口：%4")
            .arg(goalCn)
            .arg(cal)
            .arg(m_user.preferences.isEmpty() ? QStringLiteral("未设置") : m_user.preferences)
            .arg(m_user.allergens.isEmpty() ? QStringLiteral("未设置") : m_user.allergens));
}

void RecommendWidget::setAiPanelVisible(bool visible)
{
    if (m_aiVisible == visible)
        return;
    m_aiVisible = visible;
    m_aiPanel->setVisible(visible);
    m_toggleAiBtn->setText(visible ? QStringLiteral("隐藏助手") : QStringLiteral("显示助手"));
    rebuildMealLayout(!visible);
    emit aiPanelVisibilityChanged(visible);
}

void RecommendWidget::toggleAiPanel()
{
    setAiPanelVisible(!m_aiVisible);
}

void RecommendWidget::rebuildMealLayout(bool wide)
{
    // 助手隐藏：三餐横排更宽；助手显示：竖排滚动，避免与问答抢宽度
    m_mealLay->setDirection(wide ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
}

void RecommendWidget::addChatBubble(const QString &text, bool fromUser)
{
    auto *row = new QWidget(m_chatHost);
    auto *rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->setSpacing(0);

    auto *bubble = new QFrame(row);
    bubble->setObjectName(fromUser ? QStringLiteral("ChatBubbleUser") : QStringLiteral("ChatBubbleAi"));
    auto *lay = new QVBoxLayout(bubble);
    lay->setContentsMargins(12, 10, 12, 10);
    lay->setSpacing(4);

    auto *who = new QLabel(fromUser ? QStringLiteral("我") : QStringLiteral("膳衡 AI"), bubble);
    who->setObjectName(QStringLiteral("ChatBubbleWho"));
    auto *body = new QLabel(text, bubble);
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->setObjectName(QStringLiteral("ChatBubbleText"));

    lay->addWidget(who);
    lay->addWidget(body);

    if (fromUser) {
        rowLay->addStretch();
        rowLay->addWidget(bubble, 0, Qt::AlignRight);
    } else {
        rowLay->addWidget(bubble, 0, Qt::AlignLeft);
        rowLay->addStretch();
    }

    const int stretchIndex = m_chatLay->count() - 1;
    m_chatLay->insertWidget(qMax(0, stretchIndex), row);

    QTimer::singleShot(0, this, [this]() {
        if (m_chatScroll && m_chatScroll->verticalScrollBar())
            m_chatScroll->verticalScrollBar()->setValue(m_chatScroll->verticalScrollBar()->maximum());
    });
}

void RecommendWidget::setChatBusy(bool busy)
{
    m_sendBtn->setEnabled(!busy);
    m_chatInput->setEnabled(!busy);
    m_sendBtn->setText(busy ? QStringLiteral("…") : QStringLiteral("发送"));
}

void RecommendWidget::onSuggestionClicked()
{
    auto *btn = qobject_cast<QPushButton *>(sender());
    if (!btn)
        return;
    m_chatInput->setText(btn->text());
    onSendChat();
}

void RecommendWidget::onSendChat()
{
    const QString text = m_chatInput->text().trimmed();
    if (text.isEmpty())
        return;
    if (!m_aiVisible)
        setAiPanelVisible(true);
    m_lastUserMessage = text;
    addChatBubble(text, true);
    m_chatInput->clear();
    setChatBusy(true);
    m_providerLabel->setText(QStringLiteral("正在思考…"));
    m_ai->analyzeUserMessage(m_user, text, m_plan);
}

QStringList RecommendWidget::detectAllergyMentions(const QString &message) const
{
    QStringList found;
    const QStringList candidates = {
        QStringLiteral("豆制品"), QStringLiteral("大豆"), QStringLiteral("豆腐"), QStringLiteral("花生"),
        QStringLiteral("坚果"),   QStringLiteral("鸡蛋"), QStringLiteral("牛奶"), QStringLiteral("海鲜"),
        QStringLiteral("贝类"),   QStringLiteral("芝麻"), QStringLiteral("猕猴桃"), QStringLiteral("麸质"),
        QStringLiteral("小麦"),   QStringLiteral("虾"),   QStringLiteral("蟹"),   QStringLiteral("鱼"),
    };
    const bool allergyCtx = message.contains(QStringLiteral("过敏")) || message.contains(QStringLiteral("不吃"))
                            || message.contains(QStringLiteral("忌口")) || message.contains(QStringLiteral("不能吃"));
    if (!allergyCtx)
        return found;
    for (const QString &c : candidates) {
        if (message.contains(c))
            found.append(c);
    }
    // 「我对X过敏」兜底：取“过敏”前常见词
    if (found.isEmpty() && message.contains(QStringLiteral("过敏"))) {
        if (message.contains(QStringLiteral("豆")))
            found.append(QStringLiteral("豆制品"));
    }
    found.removeDuplicates();
    return found;
}

void RecommendWidget::onAiFinished(const AiPreferenceUpdate &result)
{
    setChatBusy(false);
    if (!result.ok) {
        addChatBubble(result.error.isEmpty() ? QStringLiteral("调用失败。") : result.error, false);
        m_providerLabel->setText(QStringLiteral("调用失败"));
        return;
    }

    addChatBubble(result.reply, false);
    m_providerLabel->setText(
        result.provider == QLatin1String("siliconflow")
            ? QStringLiteral("来源：硅基流动 · Qwen2.5-7B")
            : QStringLiteral("来源：本地 Ollama · qwen2.5:7b"));

    AiPreferenceUpdate out = result;
    // 过敏相关：先确认，再写入档案并触发重生成
    QStringList mentions = detectAllergyMentions(m_lastUserMessage);
    if (mentions.isEmpty() && !result.allergens.isEmpty())
        mentions = User::splitLegacyText(result.allergens);

    QStringList confirmed;
    const QStringList existing = m_user.allergies.isEmpty() ? User::splitLegacyText(m_user.allergens)
                                                           : m_user.allergies;
    for (const QString &item : mentions) {
        if (existing.contains(item))
            continue;
        const auto ans = QMessageBox::question(
            this,
            QStringLiteral("加入过敏原？"),
            QStringLiteral("是否将「%1」加入过敏原？\n加入后推荐将自动避开相关食谱（如豆腐、豆浆等）。")
                .arg(item),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);
        if (ans == QMessageBox::Yes)
            confirmed.append(item);
    }

    if (!confirmed.isEmpty()) {
        QStringList merged = existing + confirmed;
        merged.removeDuplicates();
        out.allergens = User::joinLegacyText(merged);
        out.regenerate = true;
        addChatBubble(QStringLiteral("已将「%1」加入过敏原，正在按新忌口重新生成方案…")
                          .arg(confirmed.join(QStringLiteral("、"))),
                      false);
    } else {
        // 未确认则不因模型臆测写入过敏原
        out.allergens.clear();
        if (!result.goal.isEmpty() || !result.preferences.isEmpty()) {
            // 其他档案变更仍可应用
        } else if (!out.regenerate) {
            // 纯问答
        } else if (detectAllergyMentions(m_lastUserMessage).isEmpty()
                   && !m_lastUserMessage.contains(QStringLiteral("重新生成"))
                   && !m_lastUserMessage.contains(QStringLiteral("换一套"))) {
            out.regenerate = false;
        }
    }

    emit aiPreferenceApplied(out);
}
