#include "RecipeEditorDialog.h"
#include "UiAssets.h"
#include "../services/IngredientMeasureService.h"
#include "../services/WebRecipeImportService.h"

#include "../dao/PersonalRecipeDAO.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineView>

RecipeEditorDialog::RecipeEditorDialog(int userId, Mode mode, QWidget *parent)
    : QDialog(parent), m_userId(userId), m_mode(mode)
{
    setObjectName(QStringLiteral("RecipeEditorDialog"));
    setWindowTitle(mode == Mode::WebImport ? QStringLiteral("网页食谱导入")
                                           : QStringLiteral("手动创建食谱"));
    setModal(true);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    resize(mode == Mode::WebImport ? 760 : 700, mode == Mode::WebImport ? 760 : 650);
    setMinimumSize(mode == Mode::WebImport ? QSize(680, 660) : QSize(640, 580));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(26, 22, 26, 22);
    root->setSpacing(13);

    auto *title = new QLabel(mode == Mode::WebImport ? QStringLiteral("网页食谱一键导入")
                                                      : QStringLiteral("创建家庭食谱"), this);
    title->setObjectName(QStringLiteral("DialogTitle"));
    title->setFont(UiAssets::titleFont(22));
    auto *closeTop = new QPushButton(this);
    closeTop->setObjectName(QStringLiteral("DialogCloseButton"));
    closeTop->setFixedSize(38, 38);
    closeTop->setCursor(Qt::PointingHandCursor);
    closeTop->setToolTip(QStringLiteral("关闭"));
    UiAssets::setButtonIcon(closeTop, QStringLiteral("close"), 18);
    auto *header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(closeTop, 0, Qt::AlignTop);
    auto *subtitle = new QLabel(
        mode == Mode::WebImport
            ? QStringLiteral("读取网页中的结构化食谱；保存前可修改菜名、原料和步骤。")
            : QStringLiteral("录入自己的拿手菜。所有主料都会完整保存，微量调味料不计营养。"),
        this);
    subtitle->setObjectName(QStringLiteral("DialogSubtitle"));
    subtitle->setWordWrap(true);
    root->addLayout(header);
    root->addWidget(subtitle);
    connect(closeTop, &QPushButton::clicked, this, &QDialog::reject);

    if (mode == Mode::WebImport) {
        auto *urlRow = new QHBoxLayout;
        m_url = new QLineEdit(this);
        m_url->setObjectName(QStringLiteral("WebRecipeUrl"));
        m_url->setPlaceholderText(QStringLiteral("粘贴下厨房或其他食谱网页链接"));
        m_url->setClearButtonEnabled(true);
        m_fetch = new QPushButton(QStringLiteral("读取网页"), this);
        m_fetch->setProperty("class", QStringLiteral("PrimaryButton"));
        UiAssets::setButtonIcon(m_fetch, QStringLiteral("refresh"), 17, QColor(Qt::white));
        urlRow->addWidget(m_url, 1);
        urlRow->addWidget(m_fetch);
        root->addLayout(urlRow);
        m_importStatus = new QLabel(
            QStringLiteral("支持公开页面中的 Schema.org Recipe 数据；遇到登录或滑块时可复制网页正文继续导入。"), this);
        m_importStatus->setObjectName(QStringLiteral("DialogHint"));
        m_importStatus->setWordWrap(true);
        root->addWidget(m_importStatus);

        m_fallbackToggle = new QPushButton(QStringLiteral("页面需要登录或验证？改用复制正文"), this);
        m_fallbackToggle->setProperty("class", QStringLiteral("GhostButton"));
        m_fallbackToggle->setCheckable(true);
        root->addWidget(m_fallbackToggle, 0, Qt::AlignLeft);

        m_pastePanel = new QFrame(this);
        m_pastePanel->setObjectName(QStringLiteral("WebImportFallback"));
        auto *pasteLayout = new QVBoxLayout(m_pastePanel);
        pasteLayout->setContentsMargins(14, 12, 14, 12);
        pasteLayout->setSpacing(8);
        auto *pasteHint = new QLabel(
            QStringLiteral("可在应用内打开网页完成登录或滑块验证并直接提取；也可复制包含菜名、用料和做法的正文后粘贴。"),
            m_pastePanel);
        pasteHint->setObjectName(QStringLiteral("WebImportFallbackHint"));
        pasteHint->setWordWrap(true);
        pasteLayout->addWidget(pasteHint);
        m_pastedRecipe = new QPlainTextEdit(m_pastePanel);
        m_pastedRecipe->setObjectName(QStringLiteral("WebRecipeCopiedText"));
        m_pastedRecipe->setPlaceholderText(
            QStringLiteral("例如：\n宫保鸡丁\n用料\n鸡胸肉 300g\n花生 50g\n做法\n1. 鸡肉切丁……"));
        m_pastedRecipe->setMinimumHeight(105);
        m_pastedRecipe->setMaximumHeight(145);
        pasteLayout->addWidget(m_pastedRecipe);
        auto *pasteActions = new QHBoxLayout;
        auto *openBrowser = new QPushButton(QStringLiteral("打开网页并完成验证"), m_pastePanel);
        openBrowser->setObjectName(QStringLiteral("WebRecipeOpenVerifiedPage"));
        openBrowser->setProperty("class", QStringLiteral("GhostButton"));
        auto *parsePaste = new QPushButton(QStringLiteral("解析粘贴内容"), m_pastePanel);
        parsePaste->setProperty("class", QStringLiteral("PrimaryButton"));
        pasteActions->addWidget(openBrowser);
        pasteActions->addStretch();
        pasteActions->addWidget(parsePaste);
        pasteLayout->addLayout(pasteActions);
        m_pastePanel->setVisible(false);
        root->addWidget(m_pastePanel);

        m_network = new QNetworkAccessManager(this);
        connect(m_fetch, &QPushButton::clicked, this, &RecipeEditorDialog::fetchWebRecipe);
        connect(m_network, &QNetworkAccessManager::finished, this, &RecipeEditorDialog::handleWebReply);
        connect(m_fallbackToggle, &QPushButton::toggled, this, &RecipeEditorDialog::showPasteFallback);
        connect(openBrowser, &QPushButton::clicked, this, &RecipeEditorDialog::openWebRecipeInBrowser);
        connect(parsePaste, &QPushButton::clicked, this, &RecipeEditorDialog::parsePastedRecipe);
    }

    auto *contentScroll = new QScrollArea(this);
    contentScroll->setObjectName(QStringLiteral("RecipeEditorContentScroll"));
    contentScroll->setWidgetResizable(true);
    contentScroll->setFrameShape(QFrame::NoFrame);
    contentScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    contentScroll->setMinimumHeight(330);
    auto *contentHost = new QWidget(contentScroll);
    contentHost->setObjectName(QStringLiteral("RecipeEditorContentHost"));
    auto *contentRoot = new QVBoxLayout(contentHost);
    contentRoot->setContentsMargins(0, 0, 6, 0);
    contentRoot->setSpacing(13);
    contentRoot->setSizeConstraint(QLayout::SetMinimumSize);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);
    m_name = new QLineEdit(this);
    m_name->setObjectName(QStringLiteral("WebRecipeName"));
    m_name->setPlaceholderText(QStringLiteral("例如：家常番茄炒蛋"));
    m_category = new QComboBox(this);
    m_category->addItems({QStringLiteral("荤菜"), QStringLiteral("素菜"),
                          QStringLiteral("主食"), QStringLiteral("汤羹"),
                          QStringLiteral("甜品")});
    m_category->setCurrentText(QStringLiteral("荤菜"));
    m_minutes = new QSpinBox(this);
    m_minutes->setRange(1, 360);
    m_minutes->setValue(20);
    m_minutes->setSuffix(QStringLiteral(" 分钟"));
    auto *metaRow = new QWidget(this);
    auto *metaLayout = new QHBoxLayout(metaRow);
    metaLayout->setContentsMargins(0, 0, 0, 0);
    metaLayout->setSpacing(10);
    metaLayout->addWidget(m_category, 1);
    metaLayout->addWidget(m_minutes, 1);
    form->addRow(QStringLiteral("菜品名称"), m_name);
    form->addRow(QStringLiteral("分类 / 用时"), metaRow);
    contentRoot->addLayout(form);

    auto *ingredientLabel = new QLabel(QStringLiteral("原料清单"), this);
    ingredientLabel->setObjectName(QStringLiteral("FormSectionTitle"));
    m_ingredients = new QPlainTextEdit(this);
    m_ingredients->setObjectName(QStringLiteral("WebRecipeIngredients"));
    m_ingredients->setPlaceholderText(
        QStringLiteral("每行一种原料，例如：\n茭白 4根\n鸡腿 200g\n白糖 一茶勺\n八角 2g"));
    m_ingredients->setMinimumHeight(130);
    auto *ingredientHint = new QLabel(
        QStringLiteral("可填“4根”或“一茶勺”，系统会估算克重用于营养计算并保留原单位。"
                       "请完整填写主料；八角、桂皮等香料建议保留。"), this);
    ingredientHint->setObjectName(QStringLiteral("DialogHint"));
    contentRoot->addWidget(ingredientLabel);
    contentRoot->addWidget(m_ingredients, 1);
    contentRoot->addWidget(ingredientHint);

    auto *stepsLabel = new QLabel(QStringLiteral("制作步骤"), this);
    stepsLabel->setObjectName(QStringLiteral("FormSectionTitle"));
    m_steps = new QPlainTextEdit(this);
    m_steps->setObjectName(QStringLiteral("WebRecipeSteps"));
    m_steps->setPlaceholderText(QStringLiteral("每一步单独一行，系统会自动编号。"));
    m_steps->setMinimumHeight(130);
    contentRoot->addWidget(stepsLabel);
    contentRoot->addWidget(m_steps, 1);
    contentScroll->setWidget(contentHost);
    root->addWidget(contentScroll, 1);

    auto *actions = new QHBoxLayout;
    actions->addStretch();
    auto *cancel = new QPushButton(QStringLiteral("取消"), this);
    cancel->setProperty("class", QStringLiteral("GhostButton"));
    auto *save = new QPushButton(QStringLiteral("保存到个人食谱库"), this);
    save->setProperty("class", QStringLiteral("PrimaryButton"));
    UiAssets::setButtonIcon(save, QStringLiteral("check"), 17, QColor(Qt::white));
    actions->addWidget(cancel);
    actions->addWidget(save);
    root->addLayout(actions);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(save, &QPushButton::clicked, this, &RecipeEditorDialog::saveRecipe);
}

void RecipeEditorDialog::setImportBusy(bool busy, const QString &message)
{
    if (m_fetch) {
        m_fetch->setEnabled(!busy);
        m_fetch->setText(busy ? QStringLiteral("正在读取…") : QStringLiteral("读取网页"));
    }
    if (m_url)
        m_url->setEnabled(!busy);
    if (m_importStatus && !message.isEmpty())
        m_importStatus->setText(message);
}

void RecipeEditorDialog::fetchWebRecipe()
{
    const QUrl url = QUrl::fromUserInput(m_url->text().trimmed());
    if (!url.isValid() || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))) {
        QMessageBox::information(this, QStringLiteral("链接无效"), QStringLiteral("请输入完整的 http 或 https 网页地址。"));
        return;
    }
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                                       "(KHTML, like Gecko) Chrome/124.0 Safari/537.36");
    request.setRawHeader("Accept", "text/html,application/xhtml+xml,application/ld+json;q=0.9,*/*;q=0.5");
    request.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9");
    QNetworkReply *reply = m_network->get(request);
    reply->setProperty("sourceUrl", url.toString());
    setImportBusy(true, QStringLiteral("正在读取网页并识别菜名、原料和制作步骤…"));
    QTimer::singleShot(15000, reply, [reply]() {
        if (reply->isRunning())
            reply->abort();
    });
}

void RecipeEditorDialog::handleWebReply(QNetworkReply *reply)
{
    const QString sourceUrl = reply->property("sourceUrl").toString();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    const QByteArray body = reply->readAll();
    WebRecipeImportResult result = WebRecipeImportService::parseHtml(
        body, reply->url().isValid() ? reply->url().toString() : sourceUrl, httpStatus, contentType);
    if (reply->error() != QNetworkReply::NoError && body.trimmed().isEmpty()) {
        result.state = WebRecipeImportResult::State::AccessBlocked;
        result.message = QStringLiteral(
            "网页读取失败：%1。可在浏览器正常打开页面，完成验证后复制食谱正文并粘贴解析。")
                             .arg(reply->errorString());
    }
    applyImportResult(result);
    setImportBusy(false, result.message);
    if (result.isRestricted() || result.state == WebRecipeImportResult::State::InvalidContent)
        showPasteFallback(true);
    reply->deleteLater();
}

void RecipeEditorDialog::applyImportResult(const WebRecipeImportResult &result)
{
    if (!result.name.isEmpty())
        m_name->setText(result.name);
    if (!result.ingredients.isEmpty())
        m_ingredients->setPlainText(result.ingredients.join(QLatin1Char('\n')));
    if (!result.steps.isEmpty())
        m_steps->setPlainText(result.steps.join(QLatin1Char('\n')));
    if (result.minutes > 0)
        m_minutes->setValue(result.minutes);
    if (!result.category.isEmpty() && m_category->findText(result.category) >= 0)
        m_category->setCurrentText(result.category);
}

void RecipeEditorDialog::showPasteFallback(bool visible)
{
    if (!m_pastePanel)
        return;
    m_pastePanel->setVisible(visible);
    if (m_fallbackToggle && m_fallbackToggle->isChecked() != visible)
        m_fallbackToggle->setChecked(visible);
    if (m_fallbackToggle) {
        m_fallbackToggle->setText(visible ? QStringLiteral("收起复制正文导入")
                                          : QStringLiteral("页面需要登录或验证？改用复制正文"));
    }
    if (visible && height() < 820)
        resize(width(), 820);
}

void RecipeEditorDialog::parsePastedRecipe()
{
    if (!m_pastedRecipe)
        return;
    if (m_pastedRecipe->toPlainText().trimmed().isEmpty()) {
        const QString clipboardText = QApplication::clipboard()->text().trimmed();
        if (!clipboardText.isEmpty())
            m_pastedRecipe->setPlainText(clipboardText);
    }
    const WebRecipeImportResult result =
        WebRecipeImportService::parseCopiedText(m_pastedRecipe->toPlainText());
    applyImportResult(result);
    setImportBusy(false, result.message);
    if (result.state == WebRecipeImportResult::State::InvalidContent)
        QMessageBox::information(this, QStringLiteral("没有可解析的正文"), result.message);
}

void RecipeEditorDialog::openWebRecipeInBrowser()
{
    const QUrl url = QUrl::fromUserInput(m_url ? m_url->text().trimmed() : QString());
    if (!url.isValid() || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))) {
        QMessageBox::information(this, QStringLiteral("链接无效"),
                                 QStringLiteral("请先输入完整的 http 或 https 网页地址。"));
        return;
    }

    auto *browserDialog = new QDialog(this);
    browserDialog->setAttribute(Qt::WA_DeleteOnClose);
    browserDialog->setWindowTitle(QStringLiteral("网页验证与食谱提取"));
    browserDialog->setModal(true);
    browserDialog->resize(1120, 800);
    browserDialog->setMinimumSize(820, 620);

    auto *layout = new QVBoxLayout(browserDialog);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *hint = new QLabel(
        QStringLiteral("如网页要求登录或滑块验证，请在下方正常完成。页面显示食谱后，点击“验证完成并提取本页”。"),
        browserDialog);
    hint->setObjectName(QStringLiteral("DialogHint"));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *webView = new QWebEngineView(browserDialog);
    webView->setObjectName(QStringLiteral("WebRecipeVerificationView"));
    layout->addWidget(webView, 1);

    auto *status = new QLabel(QStringLiteral("正在打开网页…"), browserDialog);
    status->setObjectName(QStringLiteral("DialogHint"));
    status->setWordWrap(true);
    auto *reload = new QPushButton(QStringLiteral("刷新页面"), browserDialog);
    reload->setProperty("class", QStringLiteral("GhostButton"));
    auto *extract = new QPushButton(QStringLiteral("验证完成并提取本页"), browserDialog);
    extract->setObjectName(QStringLiteral("WebRecipeExtractVerifiedPage"));
    extract->setProperty("class", QStringLiteral("PrimaryButton"));
    auto *close = new QPushButton(QStringLiteral("关闭"), browserDialog);
    close->setProperty("class", QStringLiteral("GhostButton"));

    auto *actions = new QHBoxLayout;
    actions->addWidget(status, 1);
    actions->addWidget(reload);
    actions->addWidget(extract);
    actions->addWidget(close);
    layout->addLayout(actions);

    connect(reload, &QPushButton::clicked, webView, &QWebEngineView::reload);
    connect(close, &QPushButton::clicked, browserDialog, &QDialog::reject);
    connect(webView, &QWebEngineView::loadStarted, browserDialog, [status]() {
        status->setText(QStringLiteral("网页加载中…"));
    });
    connect(webView, &QWebEngineView::loadFinished, browserDialog, [status](bool ok) {
        status->setText(ok ? QStringLiteral("网页已加载；完成验证并看到食谱后即可提取。")
                           : QStringLiteral("网页加载失败，可刷新重试或返回使用复制正文导入。"));
    });

    const QPointer<RecipeEditorDialog> owner(this);
    const QPointer<QDialog> browserGuard(browserDialog);
    const QPointer<QWebEngineView> webGuard(webView);
    connect(extract, &QPushButton::clicked, browserDialog,
            [owner, browserGuard, webGuard, status, extract]() {
        if (!owner || !browserGuard || !webGuard)
            return;
        extract->setEnabled(false);
        status->setText(QStringLiteral("正在从当前页面提取食谱…"));
        const QString currentUrl = webGuard->url().toString();
        webGuard->page()->toHtml(
            [owner, browserGuard, status, extract, currentUrl](const QString &html) {
                if (!owner || !browserGuard)
                    return;
                const WebRecipeImportResult result = WebRecipeImportService::parseHtml(
                    html.toUtf8(), currentUrl, 200, QStringLiteral("text/html; charset=utf-8"));
                owner->applyImportResult(result);
                owner->setImportBusy(false, result.message);
                extract->setEnabled(true);
                if (result.isComplete()) {
                    status->setText(QStringLiteral("提取成功，菜名、原料和步骤已回填。"));
                    QTimer::singleShot(650, browserGuard, [browserGuard]() {
                        if (browserGuard)
                            browserGuard->accept();
                    });
                    return;
                }
                if (!result.name.isEmpty() || !result.ingredients.isEmpty() || !result.steps.isEmpty()) {
                    status->setText(QStringLiteral("已提取页面中的可用内容；缺失部分可返回后手动补充。"));
                    return;
                }
                status->setText(QStringLiteral("当前仍是登录/验证页，或网页未公开食谱数据。请完成验证并打开具体菜谱后重试。"));
            });
    });

    webView->load(url);
    browserDialog->show();
}

void RecipeEditorDialog::saveRecipe()
{
    if (m_userId <= 0)
        return;
    const QString name = m_name->text().trimmed();
    const QStringList rawLines = m_ingredients->toPlainText().split(
        QRegularExpression(QStringLiteral("[\\r\\n;；]+")), Qt::SkipEmptyParts);
    QList<PersonalRecipeIngredient> ingredients;
    QStringList estimated;
    QStringList converted;
    for (QString line : rawLines) {
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        PersonalRecipeIngredient ingredient;
        const IngredientMeasureEstimate estimate = IngredientMeasureService::parse(line);
        if (estimate.valid) {
            ingredient.name = estimate.ingredientName;
            ingredient.quantity = estimate.grams;
            ingredient.quantityText = estimate.quantityText;
            if (estimate.estimated)
                converted.append(QStringLiteral("%1：%2").arg(ingredient.name, ingredient.quantityText));
        } else {
            ingredient.name = line;
            ingredient.quantity = 100.0;
            ingredient.quantityText = QStringLiteral("100g（估算）");
            estimated.append(line);
        }
        if (!ingredient.name.isEmpty())
            ingredients.append(ingredient);
    }
    if (name.isEmpty() || ingredients.isEmpty() || m_steps->toPlainText().trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("信息不完整"),
                                 QStringLiteral("请填写菜品名称、至少一种主料和制作步骤。"));
        return;
    }
    if (!estimated.isEmpty() || !converted.isEmpty()) {
        QStringList sections;
        if (!converted.isEmpty())
            sections.append(QStringLiteral("以下用量已按平均重量换算：\n%1")
                                .arg(converted.join(QLatin1Char('\n'))));
        if (!estimated.isEmpty())
            sections.append(QStringLiteral("以下原料未写用量，将按每项100g估算：\n%1")
                                .arg(estimated.join(QStringLiteral("、"))));
        const QString message = sections.join(QStringLiteral("\n\n"))
                                + QStringLiteral("\n\n营养价值将使用换算后的克数计算。是否继续保存？");
        if (QMessageBox::question(this, QStringLiteral("确认估算用量"), message) != QMessageBox::Yes)
            return;
    }
    QStringList stepLines = m_steps->toPlainText().split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                                                        Qt::SkipEmptyParts);
    for (int i = 0; i < stepLines.size(); ++i) {
        stepLines[i].remove(QRegularExpression(QStringLiteral("^\\s*\\d+[.、。)]\\s*")));
        stepLines[i] = QStringLiteral("%1. %2").arg(i + 1).arg(stepLines.at(i).trimmed());
    }
    PersonalRecipeDraft draft;
    draft.name = name;
    const QString classLabel = m_category->currentText();
    const QHash<QString, QString> roles = {
        {QStringLiteral("荤菜"), QStringLiteral("meat")},
        {QStringLiteral("素菜"), QStringLiteral("vegetable")},
        {QStringLiteral("主食"), QStringLiteral("staple")},
        {QStringLiteral("汤羹"), QStringLiteral("soup")},
        {QStringLiteral("甜品"), QStringLiteral("dessert")},
    };
    draft.dishRole = roles.value(classLabel, QStringLiteral("mixed"));
    draft.category = classLabel == QStringLiteral("甜品") ? QStringLiteral("早餐")
                    : classLabel == QStringLiteral("汤羹") || classLabel == QStringLiteral("素菜")
                        ? QStringLiteral("晚餐") : QStringLiteral("午餐");
    draft.cookMinutes = m_minutes->value();
    draft.steps = stepLines.join(QLatin1Char('\n'));
    draft.ingredients = ingredients;
    draft.sourceType = m_mode == Mode::WebImport ? QStringLiteral("web") : QStringLiteral("manual");
    draft.sourceUrl = m_url ? m_url->text().trimmed() : QString();
    QString error;
    const int recipeId = PersonalRecipeDAO().create(m_userId, draft, &error);
    if (recipeId <= 0) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), error);
        return;
    }
    emit recipeCreated(recipeId);
    QMessageBox::information(this, QStringLiteral("保存成功"),
                             QStringLiteral("食谱已加入个人食谱库并自动收藏。"));
    accept();
}
