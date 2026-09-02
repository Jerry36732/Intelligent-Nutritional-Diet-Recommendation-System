#include "RecommendWidget.h"
#include "RecipeCard.h"
#include "UiAssets.h"

#include "../services/UserService.h"

#include <QBoxLayout>
#include <QFrame>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QTimer>
#include <QTime>
#include <QVBoxLayout>

RecommendWidget::RecommendWidget(QWidget *parent)
    : QWidget(parent)
    , m_ai(new AiAssistantService(this))
    , m_imageAi(new NutritionAiService(this))
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(25, 4, 29, 56);
    root->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("智能推荐"), this);
    title->setProperty("class", QVariant(QStringLiteral("SectionTitle")));
    auto *intro = new QLabel(
        QStringLiteral("根据营养目标、饮食偏好和风险信息生成三餐；营养咨询为辅助功能。"),
        this);
    intro->setWordWrap(true);
    intro->setProperty("class", QVariant(QStringLiteral("HintText")));
    m_toggleAiBtn = new QPushButton(QStringLiteral("隐藏助手"), this);
    m_toggleAiBtn->setCursor(Qt::PointingHandCursor);
    m_toggleAiBtn->setProperty("class", QVariant(QStringLiteral("GhostButton")));
    m_toggleAiBtn->setToolTip(QStringLiteral("隐藏/显示 AI 助手；也可点击侧栏 Logo"));
    // 页面标题由 MainWindow 顶栏统一呈现，避免页面内重复标题。
    title->hide();
    intro->hide();
    m_toggleAiBtn->hide();

    m_split = new QHBoxLayout;
    m_split->setSpacing(23);

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
    m_generateBtn->setProperty("class", QVariant(QStringLiteral("PrimaryButton")));
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
    mealScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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

    // ---- 营养助手：页面唯一主工作区 ----
    m_aiPanel = new QFrame(this);
    m_aiPanel->setObjectName(QStringLiteral("AiAssistantPanel"));
    m_aiPanel->setProperty("class", QVariant(QStringLiteral("PanelCard")));
    m_aiPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *rightLay = new QVBoxLayout(m_aiPanel);
    rightLay->setContentsMargins(18, 24, 18, 32);
    rightLay->setSpacing(14);

    auto *aiHeaderRow = new QHBoxLayout;
    auto *rightTitle = new QLabel(QStringLiteral("营养助手"), m_aiPanel);
    rightTitle->setProperty("class", QVariant(QStringLiteral("SectionTitle")));
    auto *providerDot = new QFrame(m_aiPanel);
    providerDot->setObjectName(QStringLiteral("ProviderStatusDot"));
    providerDot->setFixedSize(8, 8);
    m_providerLabel = new QLabel(QStringLiteral("基于你的健康档案回答"), m_aiPanel);
    m_providerLabel->setProperty("class", QVariant(QStringLiteral("HintText")));
    auto *closeAiBtn = new QPushButton(QStringLiteral("收起"), m_aiPanel);
    closeAiBtn->setCursor(Qt::PointingHandCursor);
    closeAiBtn->setProperty("class", QVariant(QStringLiteral("GhostButton")));
    closeAiBtn->setFixedWidth(56);
    closeAiBtn->hide();
    aiHeaderRow->addWidget(rightTitle);
    aiHeaderRow->addWidget(providerDot);
    aiHeaderRow->addWidget(m_providerLabel);
    aiHeaderRow->addStretch();

    m_chatScroll = new QScrollArea(m_aiPanel);
    m_chatScroll->setWidgetResizable(true);
    m_chatScroll->setFrameShape(QFrame::NoFrame);
    m_chatScroll->setObjectName(QStringLiteral("AiChatScroll"));
    m_chatHost = new QWidget;
    m_chatHost->setObjectName(QStringLiteral("AiChatHost"));
    m_chatLay = new QVBoxLayout(m_chatHost);
    m_chatLay->setContentsMargins(0, 10, 0, 10);
    m_chatLay->setSpacing(18);
    m_chatLay->addStretch();
    m_chatScroll->setWidget(m_chatHost);

    auto *suggestWrap = new QWidget(m_aiPanel);
    auto *suggestLay = new QVBoxLayout(suggestWrap);
    suggestLay->setContentsMargins(0, 0, 0, 11);
    suggestLay->setSpacing(6);
    auto *suggestHint = new QLabel(QStringLiteral("试试这些："), suggestWrap);
    suggestHint->setProperty("class", QVariant(QStringLiteral("HintText")));
    suggestLay->addWidget(suggestHint);
    auto *suggestRow = new QHBoxLayout;
    suggestRow->setSpacing(6);
    const QStringList suggestions = {
        QStringLiteral("今天怎么吃？"),
        QStringLiteral("根据冰箱库存推荐"),
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
    inputWrap->setMinimumHeight(62);
    auto *inputStack = new QVBoxLayout(inputWrap);
    inputStack->setContentsMargins(10, 8, 8, 8);
    inputStack->setSpacing(7);
    m_attachmentTray = new QFrame(inputWrap);
    m_attachmentTray->setObjectName(QStringLiteral("AiAttachmentTray"));
    auto *attachmentLay = new QHBoxLayout(m_attachmentTray);
    attachmentLay->setContentsMargins(10, 5, 6, 5);
    attachmentLay->setSpacing(8);
    m_attachmentLabel = new QLabel(m_attachmentTray);
    m_attachmentLabel->setObjectName(QStringLiteral("AiAttachmentLabel"));
    m_attachmentLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_clearAttachmentsBtn = new QPushButton(QStringLiteral("清空"), m_attachmentTray);
    m_clearAttachmentsBtn->setObjectName(QStringLiteral("AiAttachmentClear"));
    m_clearAttachmentsBtn->setCursor(Qt::PointingHandCursor);
    attachmentLay->addWidget(m_attachmentLabel, 1);
    attachmentLay->addWidget(m_clearAttachmentsBtn);
    m_attachmentTray->hide();

    auto *inputLay = new QHBoxLayout;
    inputLay->setContentsMargins(0, 0, 0, 0);
    inputLay->setSpacing(8);
    m_chatInput = new QLineEdit(inputWrap);
    m_chatInput->setObjectName(QStringLiteral("AiChatInput"));
    m_chatInput->setPlaceholderText(QStringLiteral("输入你的饮食问题…"));
    m_chatInput->setClearButtonEnabled(true);
    m_photoBtn = new QPushButton(QStringLiteral("识别照片"), inputWrap);
    m_photoBtn->setObjectName(QStringLiteral("AiPhotoButton"));
    m_photoBtn->setCursor(Qt::PointingHandCursor);
    m_photoBtn->setProperty("class", QVariant(QStringLiteral("GhostButton")));
    constexpr int chatActionWidth = 144;
    constexpr int chatActionHeight = 48;
    m_photoBtn->setFixedSize(chatActionWidth, chatActionHeight);
    m_photoBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_photoBtn->setToolTip(QStringLiteral("上传菜品或食材照片，识别名称、味道、用途和营养"));
    UiAssets::setButtonIcon(m_photoBtn, QStringLiteral("camera"), 18,
                            QColor(QStringLiteral("#08A96E")));
    m_sendBtn = new QPushButton(QStringLiteral("发送"), inputWrap);
    m_sendBtn->setObjectName(QStringLiteral("AiSendButton"));
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    m_sendBtn->setProperty("class", QVariant(QStringLiteral("PrimaryButton")));
    m_sendBtn->setFixedSize(chatActionWidth, chatActionHeight);
    m_sendBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    UiAssets::setButtonIcon(m_sendBtn, QStringLiteral("send.svg"), 18, QColor(Qt::white));
    inputLay->addWidget(m_chatInput, 1);
    inputLay->addWidget(m_photoBtn);
    inputLay->addWidget(m_sendBtn);
    inputStack->addWidget(m_attachmentTray);
    inputStack->addLayout(inputLay);

    rightLay->addLayout(aiHeaderRow);
    rightLay->addWidget(m_chatScroll, 1);
    rightLay->addWidget(suggestWrap);
    rightLay->addWidget(inputWrap);
    auto *scopeRow = new QWidget(m_aiPanel);
    scopeRow->setObjectName(QStringLiteral("AiScopeNote"));
    auto *scopeLay = new QHBoxLayout(scopeRow);
    scopeLay->setContentsMargins(0, 0, 0, 0);
    scopeLay->setSpacing(6);
    scopeLay->addWidget(UiAssets::createIconLabel(scopeRow, QStringLiteral("info.svg"), 16,
                                                  QColor(QStringLiteral("#718096"))));
    auto *scopeNote = new QLabel(
        QStringLiteral("推荐结果会参考你的过敏原、食物不耐受和医疗状况"), scopeRow);
    scopeNote->setObjectName(QStringLiteral("AiScopeNoteText"));
    scopeLay->addWidget(scopeNote, 1);
    rightLay->addWidget(scopeRow);

    m_planPanel->hide();
    m_split->addWidget(m_planPanel, 0);
    m_split->addWidget(m_aiPanel, 1);

    root->addLayout(m_split, 1);

    connect(m_generateBtn, &QPushButton::clicked, this, &RecommendWidget::generateRequested);
    connect(m_toggleAiBtn, &QPushButton::clicked, this, &RecommendWidget::toggleAiPanel);
    connect(closeAiBtn, &QPushButton::clicked, this, &RecommendWidget::toggleAiPanel);
    connect(m_sendBtn, &QPushButton::clicked, this, &RecommendWidget::onSendChat);
    connect(m_photoBtn, &QPushButton::clicked, this, &RecommendWidget::onAnalyzePhoto);
    connect(m_chatInput, &QLineEdit::returnPressed, this, &RecommendWidget::onSendChat);
    connect(m_chatInput, &QLineEdit::textChanged, this, [this](const QString &text) {
        Q_UNUSED(text);
        if (!m_chatBusy)
            m_sendBtn->setEnabled(!m_chatInput->text().trimmed().isEmpty()
                                  || !m_selectedImagePaths.isEmpty());
    });
    connect(m_clearAttachmentsBtn, &QPushButton::clicked, this, [this]() {
        m_selectedImagePaths.clear();
        updateAttachmentTray();
    });
    connect(m_ai, &AiAssistantService::finished, this, &RecommendWidget::onAiFinished);
    connect(m_imageAi, &NutritionAiService::foodAnalysisFinished,
            this, &RecommendWidget::onImageAnalysisFinished);

    for (RecipeCard *card : {m_breakfastCard, m_lunchCard, m_dinnerCard}) {
        connect(card, &RecipeCard::detailClicked, this, &RecommendWidget::detailRequested);
        connect(card, &RecipeCard::mealDetailRequested, this, &RecommendWidget::mealDetailRequested);
        connect(card, &RecipeCard::favoriteToggled, this, &RecommendWidget::favoriteToggled);
    }

    m_breakfastCard->clear();
    m_lunchCard->clear();
    m_dinnerCard->clear();
    m_summaryLabel->setText(QStringLiteral("尚未生成方案。可点「重新生成」，或打开 AI 助手说明忌口/偏好。"));

    addChatBubble(QStringLiteral("我今天想增肌，晚餐希望高蛋白、少油，家里还有鸡胸肉和西兰花，怎么安排？"),
                  true);
    addChatBubble(QStringLiteral("可以安排一份高蛋白晚餐：鸡胸肉炒西兰花 + 糙米饭 + 番茄豆腐汤。\n\n预计 720 kcal，蛋白质约 40 g。\n\n需要我直接加入今日方案吗？"),
                  false);
    m_sendBtn->setEnabled(false);
}

void RecommendWidget::setUser(const User &user)
{
    if (m_user.id != user.id)
        m_ai->resetConversation();
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
        m_summaryLabel->setText(
            QStringLiteral("已完成三餐配置。方案优先满足热量与蛋白质目标，并避开档案中的过敏和不耐受信息。"));
        m_reasonLabel->setText(
            QStringLiteral("推荐依据：早餐、午餐和晚餐按约 30% / 40% / 30% 分配，并结合您的健康档案进行筛选。"));
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

void RecommendWidget::setPhotoReviewState()
{
    FoodVisionResult result;
    result.ok = true;
    result.foodName = QStringLiteral("西兰花");
    result.itemType = QStringLiteral("食材");
    result.category = QStringLiteral("蔬菜");
    result.servingGrams = 300.0;
    result.calories = 102.0;
    result.protein = 8.4;
    result.carbs = 19.8;
    result.fat = 1.2;
    result.confidence = 0.86;
    result.taste = QStringLiteral("清甜、脆嫩，焯熟后口感柔和");
    result.commonUses = QStringLiteral("清炒、焯拌、烤制，也常搭配鸡胸肉或意面");
    result.nutritionHighlights = QStringLiteral("富含膳食纤维、维生素 C 和叶酸");
    result.summary = QStringLiteral("按一颗中等大小西兰花的可食部分估算。实际营养会随重量和烹调油变化。");
    result.provider = QStringLiteral("siliconflow");
    onImageAnalysisFinished(result);
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
    const int bubbleWidth = fromUser ? 660 : 620;
    bubble->setMaximumWidth(bubbleWidth);
    bubble->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto *body = new QLabel(text, bubble);
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->setObjectName(QStringLiteral("ChatBubbleText"));
    body->setMaximumWidth(bubbleWidth - 26);
    body->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    lay->addWidget(body);
    auto *time = new QLabel(fromUser ? QStringLiteral("10:30") : QStringLiteral("10:31"), bubble);
    time->setObjectName(QStringLiteral("ChatBubbleTime"));
    time->setAlignment(fromUser ? Qt::AlignRight : Qt::AlignLeft);
    lay->addWidget(time);
    bubble->setMinimumHeight(fromUser ? 72 : 110);

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
        if (m_chatHost)
            m_chatHost->adjustSize();
        if (m_chatScroll && m_chatScroll->verticalScrollBar())
            m_chatScroll->verticalScrollBar()->setValue(m_chatScroll->verticalScrollBar()->maximum());
    });
    QTimer::singleShot(120, this, [this]() {
        if (m_chatScroll && m_chatScroll->verticalScrollBar())
            m_chatScroll->verticalScrollBar()->setValue(m_chatScroll->verticalScrollBar()->maximum());
    });
}

void RecommendWidget::addPlanReadyBubble()
{
    auto *row = new QWidget(m_chatHost);
    auto *rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 0, 0, 0);

    auto *bubble = new QFrame(row);
    bubble->setObjectName(QStringLiteral("ChatPlanReady"));
    bubble->setMaximumWidth(620);
    bubble->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto *lay = new QVBoxLayout(bubble);
    lay->setContentsMargins(12, 10, 12, 10);
    lay->setSpacing(8);

    auto *message = new QLabel(QStringLiteral("新方案已生成并同步到今日方案。"), bubble);
    message->setObjectName(QStringLiteral("ChatBubbleText"));
    message->setWordWrap(true);
    lay->addWidget(message);

    auto *open = new QPushButton(QStringLiteral("前往今日方案处查看"), bubble);
    open->setObjectName(QStringLiteral("ChatPlanLink"));
    open->setCursor(Qt::PointingHandCursor);
    open->setFocusPolicy(Qt::StrongFocus);
    UiAssets::setButtonIcon(open, QStringLiteral("calendar.svg"), 18,
                            QColor(QStringLiteral("#08A96E")));
    connect(open, &QPushButton::clicked, this, &RecommendWidget::todayPlanRequested);
    lay->addWidget(open, 0, Qt::AlignLeft);

    auto *time = new QLabel(QTime::currentTime().toString(QStringLiteral("HH:mm")), bubble);
    time->setObjectName(QStringLiteral("ChatBubbleTime"));
    time->setAlignment(Qt::AlignLeft);
    lay->addWidget(time);

    rowLay->addWidget(bubble, 0, Qt::AlignLeft);
    rowLay->addStretch();
    m_chatLay->insertWidget(qMax(0, m_chatLay->count() - 1), row);

    QTimer::singleShot(0, this, [this]() {
        if (m_chatHost)
            m_chatHost->adjustSize();
        if (m_chatScroll && m_chatScroll->verticalScrollBar())
            m_chatScroll->verticalScrollBar()->setValue(m_chatScroll->verticalScrollBar()->maximum());
    });
    QTimer::singleShot(120, this, [this]() {
        if (m_chatScroll && m_chatScroll->verticalScrollBar())
            m_chatScroll->verticalScrollBar()->setValue(m_chatScroll->verticalScrollBar()->maximum());
    });
}

void RecommendWidget::setChatBusy(bool busy)
{
    m_chatBusy = busy;
    m_sendBtn->setEnabled(!busy && (!m_chatInput->text().trimmed().isEmpty()
                                    || !m_selectedImagePaths.isEmpty()));
    m_photoBtn->setEnabled(!busy);
    m_chatInput->setEnabled(!busy);
    m_clearAttachmentsBtn->setEnabled(!busy);
    m_sendBtn->setText(busy ? QStringLiteral("…") : QStringLiteral("发送"));
    if (!busy) {
        UiAssets::setButtonIcon(m_sendBtn, QStringLiteral("send.svg"), 18, QColor(Qt::white));
        UiAssets::setButtonIcon(m_photoBtn, QStringLiteral("camera"), 18,
                                QColor(QStringLiteral("#08A96E")));
    }
}

void RecommendWidget::updateAttachmentTray()
{
    const bool hasAttachments = !m_selectedImagePaths.isEmpty();
    m_attachmentTray->setVisible(hasAttachments);
    if (hasAttachments) {
        QStringList names;
        for (const QString &path : m_selectedImagePaths)
            names.append(QFileInfo(path).fileName());
        m_attachmentLabel->setText(
            QStringLiteral("已选择 %1 张图片（发送前不会上传）：%2")
                .arg(names.size()).arg(names.join(QStringLiteral("、"))));
        m_attachmentLabel->setToolTip(names.join(QStringLiteral("\n")));
    } else {
        m_attachmentLabel->clear();
    }
    if (!m_chatBusy)
        m_sendBtn->setEnabled(hasAttachments || !m_chatInput->text().trimmed().isEmpty());
}

void RecommendWidget::onAnalyzePhoto()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择菜品或食材照片（最多6张）"), {},
        QStringLiteral("图片文件 (*.jpg *.jpeg *.png *.webp *.bmp)"));
    if (paths.isEmpty())
        return;
    if (!m_aiVisible)
        setAiPanelVisible(true);
    for (const QString &path : paths) {
        if (!m_selectedImagePaths.contains(path) && m_selectedImagePaths.size() < 6)
            m_selectedImagePaths.append(path);
    }
    updateAttachmentTray();
    m_chatInput->setFocus();
    m_providerLabel->setText(QStringLiteral("图片已暂存，可继续输入问题后发送"));
}

void RecommendWidget::onImageAnalysisFinished(const FoodVisionResult &result)
{
    setChatBusy(false);
    if (!result.ok) {
        for (const QString &path : m_activeImagePaths) {
            if (!m_selectedImagePaths.contains(path))
                m_selectedImagePaths.append(path);
        }
        m_activeImagePaths.clear();
        m_activeImageQuestion.clear();
        m_activeImageUserText.clear();
        updateAttachmentTray();
        addChatBubble(QStringLiteral("照片识别失败：%1").arg(result.error), false);
        m_providerLabel->setText(QStringLiteral("照片识别失败"));
        return;
    }

    QStringList lines;
    const QList<FoodVisionItem> items = result.items.isEmpty()
        ? QList<FoodVisionItem>{FoodVisionItem{result.foodName, result.itemType, result.category,
                                               result.taste, result.commonUses,
                                               result.nutritionHighlights, result.servingGrams,
                                               result.calories, result.protein, result.carbs,
                                               result.fat, result.confidence, result.summary,
                                               result.assumptions}}
        : result.items;
    lines << QStringLiteral("识别到 %1 项：").arg(items.size());
    for (int i = 0; i < items.size(); ++i) {
        const FoodVisionItem &item = items.at(i);
        const QString type = item.itemType.isEmpty() ? QStringLiteral("菜品或食材") : item.itemType;
        lines << QStringLiteral("%1. %2（%3）· 约%4 g / %5 kcal")
                     .arg(i + 1).arg(item.foodName, type)
                     .arg(qRound(item.servingGrams)).arg(qRound(item.calories));
        if (!item.taste.isEmpty())
            lines << QStringLiteral("   味道：%1").arg(item.taste);
        if (!item.commonUses.isEmpty())
            lines << QStringLiteral("   常用于：%1").arg(item.commonUses);
        QString nutrition = QStringLiteral("   营养：蛋白质 %1 g、碳水 %2 g、脂肪 %3 g")
                                .arg(item.protein, 0, 'f', 1)
                                .arg(item.carbs, 0, 'f', 1)
                                .arg(item.fat, 0, 'f', 1);
        if (!item.nutritionHighlights.isEmpty())
            nutrition += QStringLiteral("；%1").arg(item.nutritionHighlights);
        lines << nutrition;
    }
    if (items.size() > 1) {
        lines << QStringLiteral("合计：约 %1 g，%2 kcal；蛋白质 %3 g、碳水 %4 g、脂肪 %5 g。")
                     .arg(qRound(result.servingGrams)).arg(qRound(result.calories))
                     .arg(result.protein, 0, 'f', 1).arg(result.carbs, 0, 'f', 1)
                     .arg(result.fat, 0, 'f', 1);
    }
    if (!result.summary.isEmpty())
        lines << QStringLiteral("识别说明：%1").arg(result.summary);
    if (!result.answer.isEmpty())
        lines << QStringLiteral("针对你的问题：%1").arg(result.answer);
    lines << QStringLiteral("置信度：%1%（图片估算仅供记录参考）")
                 .arg(qRound(result.confidence * 100.0));
    const QString assistantText = lines.join(QStringLiteral("\n"));
    addChatBubble(assistantText, false);
    const QString contextQuestion = m_activeImageQuestion.isEmpty()
        ? QStringLiteral("请识别这些图片中的菜品或食材") : m_activeImageQuestion;
    m_ai->rememberExchange(contextQuestion, assistantText);
    m_activeImagePaths.clear();
    m_activeImageQuestion.clear();
    m_activeImageUserText.clear();
    m_providerLabel->setText(result.provider == QLatin1String("siliconflow")
                                 ? QStringLiteral("来源：硅基流动 · 多模态识别")
                                 : QStringLiteral("来源：本地 Ollama · 多模态识别"));
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
    if (text.isEmpty() && m_selectedImagePaths.isEmpty())
        return;
    if (!m_aiVisible)
        setAiPanelVisible(true);
    QString userBubble = text;
    if (!m_selectedImagePaths.isEmpty()) {
        QStringList names;
        for (const QString &path : m_selectedImagePaths)
            names.append(QFileInfo(path).fileName());
        const QString attachmentText = QStringLiteral("附加 %1 张图片：%2")
                                           .arg(names.size()).arg(names.join(QStringLiteral("、")));
        userBubble = userBubble.isEmpty() ? attachmentText
                                          : userBubble + QStringLiteral("\n") + attachmentText;
    }
    m_lastUserMessage = text.isEmpty() ? QStringLiteral("识别图片") : text;
    addChatBubble(userBubble, true);
    m_chatInput->clear();
    if (!m_selectedImagePaths.isEmpty()) {
        m_activeImagePaths = m_selectedImagePaths;
        m_activeImageQuestion = text;
        m_activeImageUserText = userBubble;
        m_selectedImagePaths.clear();
        updateAttachmentTray();
        setChatBusy(true);
        m_providerLabel->setText(QStringLiteral("正在识别 %1 张照片…")
                                     .arg(m_activeImagePaths.size()));
        m_imageAi->describeFoodImages(m_activeImagePaths, text);
        return;
    }
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
    const bool allergyCtx = message.contains(QStringLiteral("过敏"));
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
    if (result.provider == QLatin1String("siliconflow"))
        m_providerLabel->setText(QStringLiteral("来源：硅基流动 · Qwen3-8B"));
    else if (result.provider == QLatin1String("local-rules"))
        m_providerLabel->setText(QStringLiteral("来源：本地偏好校验"));
    else
        m_providerLabel->setText(QStringLiteral("来源：本地 Ollama · qwen3-vl:8b"));

    AiPreferenceUpdate out = result;
    const bool fridgeRecommendation =
        m_lastUserMessage.contains(QStringLiteral("冰箱"))
        && (m_lastUserMessage.contains(QStringLiteral("推荐"))
            || m_lastUserMessage.contains(QStringLiteral("方案")));
    // “根据冰箱库存推荐”是明确的方案生成操作，不应被当成普通营养问答。
    if (fridgeRecommendation)
        out.regenerate = true;
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
            // 其他档案变更仍可应用，保持 regenerate
        } else if (!out.regenerate) {
            // 纯问答
        } else {
            const QString &msg = m_lastUserMessage;
            const bool asksPlanAdjust =
                msg.contains(QStringLiteral("白米饭")) || msg.contains(QStringLiteral("主食"))
                || msg.contains(QStringLiteral("习惯")) || msg.contains(QStringLiteral("调整"))
                || msg.contains(QStringLiteral("换成")) || msg.contains(QStringLiteral("改成"))
                || msg.contains(QStringLiteral("重新生成")) || msg.contains(QStringLiteral("换一套"))
                || msg.contains(QStringLiteral("换个方案")) || msg.contains(QStringLiteral("重新推荐"))
                || msg.contains(QStringLiteral("冰箱库存")) || msg.contains(QStringLiteral("根据冰箱"));
            if (detectAllergyMentions(msg).isEmpty() && !asksPlanAdjust)
                out.regenerate = false;
        }
    }

    // MainWindow 的槽为同线程直接连接：返回时用户档案和新方案已经同步完成。
    emit aiPreferenceApplied(out);
    if (out.regenerate && m_plan.valid)
        addPlanReadyBubble();
}
