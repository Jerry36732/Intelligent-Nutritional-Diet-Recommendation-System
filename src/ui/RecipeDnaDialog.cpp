#include "RecipeDnaDialog.h"

#include "UiAssets.h"
#include "../dao/PersonalRecipeDAO.h"
#include "../services/FlavorFingerprintService.h"

#include <QCloseEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QtMath>

class FlavorRadarWidget : public QWidget
{
public:
    explicit FlavorRadarWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumSize(370, 158);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    void setFingerprints(const FlavorFingerprint &before, const FlavorFingerprint &after)
    {
        m_before = before;
        m_after = after;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QPointF center(112.0, height() / 2.0);
        const qreal radius = qMin(62.0, height() / 2.0 - 20.0);
        const QStringList labels = FlavorFingerprint::labels();
        // 仅改变雷达图的空间顺序，持久化字段仍保持甜、酸、咸、辣……。
        const int axisIndexes[8] = {0, 1, 3, 2, 4, 5, 6, 7};
        double axisMaximum = 20.0;
        for (int i = 0; i < 8; ++i)
            axisMaximum = qMax(axisMaximum, qMax(m_before.value(i), m_after.value(i)));
        axisMaximum = qBound(20.0, qCeil(axisMaximum / 10.0) * 10.0, 100.0);
        auto point = [&](int index, double ratio) {
            const double angle = -M_PI_2 + index * M_PI * 2.0 / 8.0;
            return center + QPointF(qCos(angle), qSin(angle)) * radius * ratio;
        };
        painter.setFont(UiAssets::bodyFont(9));
        for (int level = 1; level <= 4; ++level) {
            QPolygonF grid;
            for (int i = 0; i < 8; ++i)
                grid << point(i, level / 4.0);
            painter.setPen(QPen(QColor(QStringLiteral("#D7E7DF")), 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawPolygon(grid);
        }
        for (int i = 0; i < 8; ++i) {
            painter.drawLine(center, point(i, 1.0));
            const QPointF labelPoint = point(i, 1.24);
            painter.setPen(QColor(QStringLiteral("#475569")));
            painter.drawText(QRectF(labelPoint.x() - 20, labelPoint.y() - 9, 40, 18),
                             Qt::AlignCenter, labels.at(axisIndexes[i]));
        }
        auto drawFingerprint = [&](const FlavorFingerprint &fingerprint,
                                   const QColor &lineColor, const QColor &fillColor) {
            QPolygonF polygon;
            for (int i = 0; i < 8; ++i) {
                const int valueIndex = axisIndexes[i];
                polygon << point(i, qBound(0.0,
                                            fingerprint.value(valueIndex) / axisMaximum, 1.0));
            }
            painter.setPen(QPen(lineColor, 2.0));
            painter.setBrush(fillColor);
            painter.drawPolygon(polygon);
        };
        drawFingerprint(m_before, QColor(QStringLiteral("#64748B")), QColor(100, 116, 139, 32));
        drawFingerprint(m_after, QColor(QStringLiteral("#059669")), QColor(5, 150, 105, 55));

        const QRect legend(224, 28, width() - 230, height() - 42);
        painter.setFont(UiAssets::bodyFont(10, QFont::DemiBold));
        painter.setPen(QColor(QStringLiteral("#0B163A")));
        painter.drawText(legend.adjusted(0, 0, 0, -64), Qt::AlignLeft | Qt::AlignTop,
                         QStringLiteral("风味强度（满刻度 %1）").arg(qRound(axisMaximum)));
        painter.setFont(UiAssets::bodyFont(9));
        for (int i = 0; i < 8; ++i) {
            const int row = i % 4;
            const int column = i / 4;
            painter.setPen(column == 0 ? QColor(QStringLiteral("#475569"))
                                       : QColor(QStringLiteral("#047857")));
            const int valueIndex = axisIndexes[i];
            const QString text = QStringLiteral("%1 %2→%3")
                .arg(labels.at(valueIndex))
                .arg(qRound(m_before.value(valueIndex)))
                .arg(qRound(m_after.value(valueIndex)));
            painter.drawText(legend.left() + column * 62, legend.top() + 30 + row * 22, text);
        }
    }

private:
    FlavorFingerprint m_before;
    FlavorFingerprint m_after;
};

RecipeDnaDialog::RecipeDnaDialog(const Recipe &recipe, int userId, QWidget *parent)
    : QDialog(parent)
    , m_recipe(recipe)
    , m_userId(userId)
    , m_service(new NutritionAiService(this))
{
    setWindowTitle(QStringLiteral("食谱DNA改造"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setObjectName(QStringLiteral("RecipeDnaDialog"));
    setModal(true);
    const QScreen *screen = parent ? parent->screen() : QGuiApplication::primaryScreen();
    const QSize available = screen ? screen->availableGeometry().size() : QSize(1200, 920);
    const QSize dialogSize(qMax(920, qMin(1100, available.width() - 32)),
                           qMax(700, qMin(900, available.height() - 32)));
    setMinimumSize(qMin(920, dialogSize.width()), qMin(700, dialogSize.height()));
    resize(dialogSize);
    m_originalFlavor = FlavorFingerprintService().forRecipe(m_recipe);
    m_transformedFlavor = m_originalFlavor;
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(30, 22, 30, 24);
    root->setSpacing(13);

    auto *header = new QHBoxLayout;
    auto *titleBox = new QVBoxLayout;
    titleBox->setSpacing(2);
    auto *title = new QLabel(QStringLiteral("食谱DNA改造"), this);
    title->setObjectName(QStringLiteral("AiDialogTitle"));
    title->setFont(UiAssets::titleFont(27));
    auto *subtitle = new QLabel(
        QStringLiteral("以「%1」为母本，调整食材与烹饪方式，同时尽量保留风味").arg(recipe.name), this);
    subtitle->setObjectName(QStringLiteral("AiDialogSubtitle"));
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);
    auto *close = new QPushButton(this);
    close->setObjectName(QStringLiteral("DialogCloseButton"));
    close->setFixedSize(38, 38);
    UiAssets::setButtonIcon(close, QStringLiteral("close"), 18);
    header->addLayout(titleBox);
    header->addStretch();
    header->addWidget(close, 0, Qt::AlignTop);
    root->addLayout(header);

    auto *presetRow = new QHBoxLayout;
    presetRow->setSpacing(8);
    addPreset(QStringLiteral("脂肪 -30%"), QStringLiteral("把总脂肪减少30%，保留原菜主要风味"), presetRow);
    addPreset(QStringLiteral("提高蛋白质"), QStringLiteral("提高优质蛋白质，热量不要明显上升"), presetRow);
    addPreset(QStringLiteral("少油少盐"), QStringLiteral("减少烹调油和钠，保持香气和口感"), presetRow);
    addPreset(QStringLiteral("素食替换"), QStringLiteral("改为蛋奶素版本，保持相近口感"), presetRow);
    presetRow->addStretch();
    root->addLayout(presetRow);

    auto *requestRow = new QHBoxLayout;
    requestRow->setSpacing(10);
    m_instruction = new QPlainTextEdit(this);
    m_instruction->setObjectName(QStringLiteral("DnaInstructionInput"));
    m_instruction->setPlaceholderText(QStringLiteral("输入改造目标，例如：把脂肪减少30%，但保留红烧风味。"));
    m_instruction->setFixedHeight(82);
    m_generate = new QPushButton(QStringLiteral("生成改造方案"), this);
    m_generate->setObjectName(QStringLiteral("AiPrimaryButton"));
    m_generate->setFixedSize(178, 52);
    UiAssets::setButtonIcon(m_generate, QStringLiteral("dna"), 18,
                            QColor(QStringLiteral("#FFFFFF")));
    m_cancelRequest = new QPushButton(QStringLiteral("取消"), this);
    m_cancelRequest->setObjectName(QStringLiteral("AiLinkButton"));
    m_cancelRequest->setVisible(false);
    auto *requestActions = new QVBoxLayout;
    requestActions->addWidget(m_generate);
    requestActions->addWidget(m_cancelRequest, 0, Qt::AlignCenter);
    requestActions->addStretch();
    requestRow->addWidget(m_instruction, 1);
    requestRow->addLayout(requestActions);
    root->addLayout(requestRow);

    auto *resultPanel = new QFrame(this);
    resultPanel->setObjectName(QStringLiteral("AiResultPanel"));
    auto *result = new QVBoxLayout(resultPanel);
    result->setContentsMargins(14, 12, 14, 12);
    result->setSpacing(8);
    auto *resultTitleRow = new QHBoxLayout;
    auto *resultTitle = new QLabel(QStringLiteral("改造方案（保存前可编辑）"), resultPanel);
    resultTitle->setObjectName(QStringLiteral("AiSectionTitle"));
    m_nutrition = new QLabel(QStringLiteral("等待生成"), resultPanel);
    m_nutrition->setObjectName(QStringLiteral("DnaNutritionBadge"));
    resultTitleRow->addWidget(resultTitle);
    resultTitleRow->addStretch();
    resultTitleRow->addWidget(m_nutrition);
    result->addLayout(resultTitleRow);
    m_name = new QLineEdit(resultPanel);
    m_name->setObjectName(QStringLiteral("AiResultInput"));
    m_name->setPlaceholderText(QStringLiteral("改造后的食谱名称"));
    result->addWidget(m_name);
    m_summary = new QLabel(QStringLiteral("AI会在此说明改造逻辑和风味保留方式。"), resultPanel);
    m_summary->setObjectName(QStringLiteral("DnaSummary"));
    m_summary->setWordWrap(true);
    result->addWidget(m_summary);

    auto *flavorCard = new QFrame(resultPanel);
    flavorCard->setObjectName(QStringLiteral("DnaFlavorCard"));
    auto *flavorLayout = new QHBoxLayout(flavorCard);
    flavorLayout->setContentsMargins(12, 8, 12, 8);
    flavorLayout->setSpacing(14);
    m_flavorRadar = new FlavorRadarWidget(flavorCard);
    m_flavorRadar->setFingerprints(m_originalFlavor, m_transformedFlavor);
    auto *flavorCopy = new QVBoxLayout;
    flavorCopy->setSpacing(6);
    auto *flavorTitle = new QLabel(QStringLiteral("风味对比"), flavorCard);
    flavorTitle->setObjectName(QStringLiteral("DnaFlavorTitle"));
    auto *flavorHint = new QLabel(
        QStringLiteral("灰色为原版，绿色为改造版。图表按甜、酸、辣、咸、鲜、香、酥脆、软糯八个维度核对。"),
        flavorCard);
    flavorHint->setObjectName(QStringLiteral("DnaFlavorHint"));
    flavorHint->setWordWrap(true);
    m_flavorSummary = new QLabel(
        FlavorFingerprintService::comparisonSummary(m_originalFlavor, m_transformedFlavor),
        flavorCard);
    m_flavorSummary->setObjectName(QStringLiteral("DnaFlavorSummary"));
    m_flavorSummary->setWordWrap(true);
    flavorCopy->addWidget(flavorTitle);
    flavorCopy->addWidget(flavorHint);
    flavorCopy->addWidget(m_flavorSummary);
    flavorCopy->addStretch();
    flavorLayout->addWidget(m_flavorRadar);
    flavorLayout->addLayout(flavorCopy, 1);
    result->addWidget(flavorCard);

    auto *columns = new QHBoxLayout;
    columns->setSpacing(12);
    auto *ingredientBox = new QVBoxLayout;
    auto *ingredientTitle = new QLabel(QStringLiteral("完整原料"), resultPanel);
    ingredientTitle->setObjectName(QStringLiteral("AiFieldLabel"));
    m_ingredients = new QTableWidget(resultPanel);
    m_ingredients->setObjectName(QStringLiteral("DnaIngredientTable"));
    m_ingredients->setColumnCount(3);
    m_ingredients->setHorizontalHeaderLabels(
        {QStringLiteral("原料"), QStringLiteral("克数"), QStringLiteral("自然单位")});
    m_ingredients->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_ingredients->setColumnWidth(1, 68);
    m_ingredients->setColumnWidth(2, 112);
    m_ingredients->verticalHeader()->hide();
    m_ingredients->setAlternatingRowColors(true);
    m_ingredients->setMinimumHeight(dialogSize.height() >= 840 ? 220 : 120);
    ingredientBox->addWidget(ingredientTitle);
    ingredientBox->addWidget(m_ingredients);
    auto *stepsBox = new QVBoxLayout;
    auto *stepsTitle = new QLabel(QStringLiteral("制作步骤"), resultPanel);
    stepsTitle->setObjectName(QStringLiteral("AiFieldLabel"));
    m_steps = new QPlainTextEdit(resultPanel);
    m_steps->setObjectName(QStringLiteral("DnaStepsInput"));
    m_steps->setPlaceholderText(QStringLiteral("生成后可在此修正步骤"));
    m_steps->setMinimumHeight(dialogSize.height() >= 840 ? 220 : 120);
    stepsBox->addWidget(stepsTitle);
    stepsBox->addWidget(m_steps);
    columns->addLayout(ingredientBox, 1);
    columns->addLayout(stepsBox, 1);
    result->addLayout(columns, 1);
    root->addWidget(resultPanel, 1);

    auto *footer = new QHBoxLayout;
    m_status = new QLabel(QStringLiteral("AI改造结果是营养估算，保存后会再按本地食材库计算。"), this);
    m_status->setObjectName(QStringLiteral("AiStatusText"));
    m_status->setWordWrap(true);
    m_save = new QPushButton(QStringLiteral("保存为个人食谱"), this);
    m_save->setObjectName(QStringLiteral("AiPrimaryButton"));
    m_save->setFixedSize(210, 48);
    m_save->setEnabled(false);
    UiAssets::setButtonIcon(m_save, QStringLiteral("check"), 18,
                            QColor(QStringLiteral("#FFFFFF")));
    footer->addWidget(m_status, 1);
    footer->addWidget(m_save);
    root->addLayout(footer);

    connect(close, &QPushButton::clicked, this, &RecipeDnaDialog::reject);
    connect(m_generate, &QPushButton::clicked, this, &RecipeDnaDialog::runTransform);
    connect(m_cancelRequest, &QPushButton::clicked, m_service, &NutritionAiService::cancel);
    connect(m_save, &QPushButton::clicked, this, &RecipeDnaDialog::saveRecipe);
    connect(m_service, &NutritionAiService::busyChanged, this, &RecipeDnaDialog::setBusy);
    connect(m_service, &NutritionAiService::recipeTransformFinished,
            this, &RecipeDnaDialog::applyResult);
}

void RecipeDnaDialog::setReviewState()
{
    m_instruction->setPlainText(QStringLiteral("把脂肪减少30%，保留原菜主要风味"));
    RecipeDnaResult result;
    result.ok = true;
    result.name = QStringLiteral("轻脂版·") + m_recipe.name;
    result.category = m_recipe.category;
    result.dishRole = m_recipe.dishRole;
    result.cookMinutes = qMax(20, m_recipe.cookMinutes);
    result.ingredients = {
        {QStringLiteral("主料（去除可见脂肪）"), 140.0, QStringLiteral("约140g")},
        {QStringLiteral("杏鲍菇"), 80.0, QStringLiteral("半根约80g")},
        {QStringLiteral("葱姜"), 12.0, QStringLiteral("适量约12g")},
    };
    result.steps = QStringLiteral("1. 主料焯水并去除可见脂肪。\n2. 少油煸香葱姜，加入主料。\n3. 加入杏鲍菇和原有香料，小火炖至入味。");
    result.calories = qMax(1.0, m_recipe.totalCalories * 0.78);
    result.protein = qMax(1.0, m_recipe.totalProtein);
    result.carbs = qMax(1.0, m_recipe.totalCarbs);
    result.fat = qMax(1.0, m_recipe.totalFat * 0.70);
    result.changeSummary = QStringLiteral("减少高脂主料并用菌菇补充鲜味和口感，采用少油煸香与小火炖煮保留风味。");
    result.provider = QStringLiteral("review");
    applyResult(result);
}

void RecipeDnaDialog::addPreset(const QString &text, const QString &instruction,
                                QHBoxLayout *layout)
{
    auto *button = new QPushButton(text, this);
    button->setObjectName(QStringLiteral("DnaPresetButton"));
    button->setFixedHeight(34);
    layout->addWidget(button);
    connect(button, &QPushButton::clicked, this, [this, instruction]() {
        m_instruction->setPlainText(instruction);
        m_instruction->setFocus();
    });
}

void RecipeDnaDialog::runTransform()
{
    const QString instruction = m_instruction->toPlainText().trimmed();
    if (instruction.isEmpty()) {
        m_status->setText(QStringLiteral("请先填写改造目标，或选择一个快捷目标。"));
        return;
    }
    m_save->setEnabled(false);
    m_status->setText(QStringLiteral("正在分析原食谱并生成完整改造方案…"));
    m_service->transformRecipe(m_recipe, instruction);
}

void RecipeDnaDialog::requestTransform(const QString &instruction)
{
    m_instruction->setPlainText(instruction);
    runTransform();
}

void RecipeDnaDialog::applyResult(const RecipeDnaResult &result)
{
    if (!result.ok) {
        m_status->setText(QStringLiteral("改造失败：%1").arg(result.error));
        m_nutrition->setText(QStringLiteral("生成失败"));
        m_save->setEnabled(false);
        m_transformedFlavor = m_originalFlavor;
        m_flavorRadar->setFingerprints(m_originalFlavor, m_originalFlavor);
        m_flavorSummary->setText(QStringLiteral("生成有效方案后显示风味相似度与维度变化。"));
        return;
    }
    m_result = result;
    m_name->setText(result.name);
    m_summary->setText(result.changeSummary.isEmpty()
                           ? QStringLiteral("已生成改造方案，请核对原料和步骤。")
                           : result.changeSummary);
    m_nutrition->setText(QStringLiteral("约 %1 kcal · 蛋白 %2g · 脂肪 %3g")
                             .arg(qRound(result.calories))
                             .arg(result.protein, 0, 'f', 1)
                             .arg(result.fat, 0, 'f', 1));
    QList<QPair<QString, double>> flavorIngredients;
    double totalWeight = 0.0;
    for (const RecipeDnaIngredient &ingredient : result.ingredients) {
        flavorIngredients.append({ingredient.name, ingredient.quantity});
        totalWeight += ingredient.quantity;
    }
    m_transformedFlavor = FlavorFingerprintService().estimate(
        result.name, flavorIngredients, result.steps, result.fat, totalWeight);
    m_flavorRadar->setFingerprints(m_originalFlavor, m_transformedFlavor);
    m_flavorSummary->setText(
        FlavorFingerprintService::comparisonSummary(m_originalFlavor, m_transformedFlavor));
    m_ingredients->setRowCount(result.ingredients.size());
    for (int row = 0; row < result.ingredients.size(); ++row) {
        const RecipeDnaIngredient &ingredient = result.ingredients.at(row);
        m_ingredients->setItem(row, 0, new QTableWidgetItem(ingredient.name));
        m_ingredients->setItem(row, 1,
                               new QTableWidgetItem(QString::number(ingredient.quantity, 'f', 1)));
        m_ingredients->setItem(row, 2, new QTableWidgetItem(ingredient.quantityText));
    }
    m_steps->setPlainText(result.steps);
    m_status->setText(QStringLiteral("改造完成。主料、克数和步骤均可编辑，确认后保存到食谱大全。"));
    m_save->setEnabled(m_userId > 0);
}

void RecipeDnaDialog::saveRecipe()
{
    PersonalRecipeDraft draft;
    draft.name = m_name->text().trimmed();
    draft.category = m_result.category;
    draft.dishRole = m_result.dishRole;
    draft.steps = m_steps->toPlainText().trimmed();
    draft.cookMinutes = m_result.cookMinutes;
    draft.sourceType = QStringLiteral("dna");
    draft.sourceUrl = QStringLiteral("recipe:%1").arg(m_recipe.id);
    for (int row = 0; row < m_ingredients->rowCount(); ++row) {
        const QTableWidgetItem *nameItem = m_ingredients->item(row, 0);
        const QTableWidgetItem *gramsItem = m_ingredients->item(row, 1);
        if (!nameItem || !gramsItem)
            continue;
        PersonalRecipeIngredient ingredient;
        ingredient.name = nameItem->text().trimmed();
        ingredient.quantity = gramsItem->text().toDouble();
        if (const QTableWidgetItem *displayItem = m_ingredients->item(row, 2))
            ingredient.quantityText = displayItem->text().trimmed();
        if (!ingredient.name.isEmpty() && ingredient.quantity > 0.0)
            draft.ingredients.append(ingredient);
    }
    if (draft.name.isEmpty() || draft.steps.isEmpty() || draft.ingredients.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("内容不完整"),
                             QStringLiteral("食谱名称、完整主料和步骤不能为空。"));
        return;
    }
    QString error;
    const int recipeId = PersonalRecipeDAO().create(m_userId, draft, &error);
    if (recipeId <= 0) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), error);
        return;
    }
    FlavorFingerprintService().persist(recipeId, m_transformedFlavor,
                                       QStringLiteral("rule-v3-texture-baseline:dna"));
    emit personalRecipeCreated(recipeId);
    QMessageBox::information(this, QStringLiteral("已保存"),
                             QStringLiteral("改造食谱已加入食谱大全和我的收藏。"));
    accept();
}

void RecipeDnaDialog::setBusy(bool busy)
{
    m_generate->setEnabled(!busy);
    m_cancelRequest->setVisible(busy);
}

void RecipeDnaDialog::closeEvent(QCloseEvent *event)
{
    shutdownRequest();
    QDialog::closeEvent(event);
}

void RecipeDnaDialog::accept()
{
    shutdownRequest();
    QDialog::accept();
}

void RecipeDnaDialog::reject()
{
    shutdownRequest();
    QDialog::reject();
}

void RecipeDnaDialog::shutdownRequest()
{
    if (m_shuttingDown)
        return;
    m_shuttingDown = true;
    if (m_service) {
        // 关闭期间不再更新即将销毁的按钮和标签。
        QObject::disconnect(m_service, nullptr, this, nullptr);
        m_service->cancel();
    }
}
