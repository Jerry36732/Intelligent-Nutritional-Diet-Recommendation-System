#include "FoodVisionDialog.h"

#include "UiAssets.h"
#include "../dao/FoodLogDAO.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QDate>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPushButton>
#include <QTime>
#include <QVBoxLayout>

FoodVisionDialog::FoodVisionDialog(int userId, QWidget *parent)
    : QDialog(parent)
    , m_userId(userId)
    , m_service(new NutritionAiService(this))
{
    setWindowTitle(QStringLiteral("食物拍照识别"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setObjectName(QStringLiteral("FoodVisionDialog"));
    setModal(true);
    setFixedSize(1000, 740);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 22, 28, 22);
    root->setSpacing(16);
    auto *header = new QHBoxLayout;
    auto *titleBox = new QVBoxLayout;
    titleBox->setSpacing(3);
    auto *title = new QLabel(QStringLiteral("食物拍照识别"), this);
    title->setObjectName(QStringLiteral("FoodVisionTitle"));
    title->setFont(UiAssets::titleFont(27));
    auto *subtitle = new QLabel(QStringLiteral("俯视图确定面积，侧视图确定高度；加入尺寸参照可进一步缩小份量区间"), this);
    subtitle->setObjectName(QStringLiteral("FoodVisionSubtitle"));
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);
    auto *close = new QPushButton(this);
    close->setObjectName(QStringLiteral("DialogCloseButton"));
    close->setFixedSize(36, 36);
    UiAssets::setButtonIcon(close, QStringLiteral("close"), 18);
    header->addLayout(titleBox);
    header->addStretch();
    header->addWidget(close, 0, Qt::AlignTop);
    root->addLayout(header);

    auto *content = new QHBoxLayout;
    content->setSpacing(22);
    auto *imagePanel = new QFrame(this);
    imagePanel->setObjectName(QStringLiteral("FoodVisionImagePanel"));
    imagePanel->setFixedWidth(390);
    auto *imageLayout = new QVBoxLayout(imagePanel);
    imageLayout->setContentsMargins(14, 14, 14, 14);
    imageLayout->setSpacing(12);
    auto *stepHint = new QLabel(QStringLiteral("1 俯视范围   ·   2 侧视高度   ·   3 参照校准"), imagePanel);
    stepHint->setObjectName(QStringLiteral("FoodVisionStepHint"));
    auto *photos = new QHBoxLayout;
    photos->setSpacing(10);
    auto makePhotoCard = [imagePanel](const QString &titleText, const QString &hintText,
                                      QLabel **previewOut, QPushButton **buttonOut) {
        auto *card = new QFrame(imagePanel);
        card->setObjectName(QStringLiteral("FoodVisionAngleCard"));
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(8, 8, 8, 8);
        cardLayout->setSpacing(6);
        auto *caption = new QLabel(titleText, card);
        caption->setObjectName(QStringLiteral("FoodVisionAngleTitle"));
        auto *preview = new QLabel(hintText, card);
        preview->setObjectName(QStringLiteral("FoodVisionAnglePreview"));
        preview->setFixedSize(158, 140);
        preview->setAlignment(Qt::AlignCenter);
        preview->setWordWrap(true);
        auto *button = new QPushButton(QStringLiteral("选择照片"), card);
        button->setObjectName(QStringLiteral("FoodVisionAngleButton"));
        button->setFixedHeight(36);
        UiAssets::setButtonIcon(button, QStringLiteral("camera"), 16,
                                QColor(QStringLiteral("#059669")));
        cardLayout->addWidget(caption);
        cardLayout->addWidget(preview);
        cardLayout->addWidget(button);
        *previewOut = preview;
        *buttonOut = button;
        return card;
    };
    QPushButton *chooseTop = nullptr;
    QPushButton *chooseSide = nullptr;
    photos->addWidget(makePhotoCard(QStringLiteral("俯视图 · 必选"),
                                    QStringLiteral("完整拍下餐盘\n保持镜头垂直"),
                                    &m_topPreview, &chooseTop));
    photos->addWidget(makePhotoCard(QStringLiteral("侧视图 · 推荐"),
                                    QStringLiteral("拍下食物高度\n尽量保持同一餐盘"),
                                    &m_sidePreview, &chooseSide));
    auto *referenceRow = new QHBoxLayout;
    auto *referenceLabel = new QLabel(QStringLiteral("尺寸参照物"), imagePanel);
    referenceLabel->setObjectName(QStringLiteral("FoodVisionFieldLabel"));
    m_reference = new QComboBox(imagePanel);
    m_reference->setObjectName(QStringLiteral("FoodVisionReference"));
    m_reference->addItems({QStringLiteral("无参照物"), QStringLiteral("银行卡（85.6 × 54 mm）"),
                           QStringLiteral("标准餐盘（直径23 cm）"), QStringLiteral("筷子（约24 cm）")});
    referenceRow->addWidget(referenceLabel);
    referenceRow->addWidget(m_reference, 1);
    m_analyze = new QPushButton(QStringLiteral("开始AI识别"), imagePanel);
    m_analyze->setObjectName(QStringLiteral("AiPrimaryButton"));
    m_analyze->setFixedHeight(42);
    m_analyze->setEnabled(false);
    UiAssets::setButtonIcon(m_analyze, QStringLiteral("recommend-star"), 18,
                            QColor(QStringLiteral("#FFFFFF")));
    m_cancelRequest = new QPushButton(QStringLiteral("取消识别"), imagePanel);
    m_cancelRequest->setObjectName(QStringLiteral("AiLinkButton"));
    m_cancelRequest->setVisible(false);
    auto *privacy = new QLabel(
        QStringLiteral("两张照片会作为同一次请求发送至当前AI服务；只有点击保存后，俯视图副本才写入本地饮食记录。"),
        imagePanel);
    privacy->setObjectName(QStringLiteral("AiPrivacyHint"));
    privacy->setWordWrap(true);
    imageLayout->addWidget(stepHint);
    imageLayout->addLayout(photos);
    imageLayout->addLayout(referenceRow);
    imageLayout->addWidget(m_analyze);
    imageLayout->addWidget(m_cancelRequest, 0, Qt::AlignCenter);
    imageLayout->addStretch();
    imageLayout->addWidget(privacy);

    auto *resultPanel = new QFrame(this);
    resultPanel->setObjectName(QStringLiteral("FoodVisionResultPanel"));
    auto *resultLayout = new QVBoxLayout(resultPanel);
    resultLayout->setContentsMargins(18, 16, 18, 16);
    resultLayout->setSpacing(11);
    auto *resultTitleRow = new QHBoxLayout;
    auto *resultTitle = new QLabel(QStringLiteral("识别结果（保存前可修改）"), resultPanel);
    resultTitle->setObjectName(QStringLiteral("FoodVisionSectionTitle"));
    m_confidence = new QLabel(QStringLiteral("等待识别"), resultPanel);
    m_confidence->setObjectName(QStringLiteral("AiConfidenceBadge"));
    resultTitleRow->addWidget(resultTitle);
    resultTitleRow->addStretch();
    resultTitleRow->addWidget(m_confidence);
    resultLayout->addLayout(resultTitleRow);
    m_rangeLabel = new QLabel(QStringLiteral("份量区间将在识别后显示"), resultPanel);
    m_rangeLabel->setObjectName(QStringLiteral("FoodVisionRangeLabel"));
    resultLayout->addWidget(m_rangeLabel);

    auto *form = new QFormLayout;
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_name = new QLineEdit(resultPanel);
    m_name->setPlaceholderText(QStringLiteral("识别后显示食物名称"));
    m_name->setObjectName(QStringLiteral("AiResultInput"));
    m_meal = new QComboBox(resultPanel);
    m_meal->setObjectName(QStringLiteral("AiResultInput"));
    m_meal->addItems({QStringLiteral("早餐"), QStringLiteral("午餐"),
                      QStringLiteral("晚餐"), QStringLiteral("加餐")});
    const int hour = QTime::currentTime().hour();
    m_meal->setCurrentText(hour < 10 ? QStringLiteral("早餐")
                           : hour < 15 ? QStringLiteral("午餐")
                           : hour < 21 ? QStringLiteral("晚餐") : QStringLiteral("加餐"));
    m_itemSelector = new QComboBox(resultPanel);
    m_itemSelector->setObjectName(QStringLiteral("AiResultInput"));
    m_itemSelector->setEnabled(false);
    auto addFormRow = [resultPanel, form](const QString &text, QWidget *field) {
        auto *label = new QLabel(text, resultPanel);
        label->setObjectName(QStringLiteral("FoodVisionFieldLabel"));
        form->addRow(label, field);
    };
    addFormRow(QStringLiteral("识别项目"), m_itemSelector);
    addFormRow(QStringLiteral("食物名称"), m_name);
    addFormRow(QStringLiteral("餐次"), m_meal);
    resultLayout->addLayout(form);

    auto *numbers = new QGridLayout;
    numbers->setHorizontalSpacing(8);
    numbers->setVerticalSpacing(8);
    auto addMetric = [&](const QString &label, QDoubleSpinBox **field, const QString &unit,
                         double maximum, int row, int column, const QString &tone) {
        auto *card = new QFrame(resultPanel);
        card->setProperty("class", QStringLiteral("FoodVisionMetric"));
        card->setProperty("tone", tone);
        auto *metricLayout = new QVBoxLayout(card);
        metricLayout->setContentsMargins(9, 7, 9, 7);
        metricLayout->setSpacing(2);
        auto *caption = new QLabel(label, card);
        caption->setObjectName(QStringLiteral("FoodVisionMetricLabel"));
        *field = createNumberField(unit, maximum, 1);
        metricLayout->addWidget(caption);
        metricLayout->addWidget(*field);
        numbers->addWidget(card, row, column);
    };
    addMetric(QStringLiteral("估算份量"), &m_grams, QStringLiteral("g"), 5000, 0, 0,
              QStringLiteral("green"));
    addMetric(QStringLiteral("总热量"), &m_calories, QStringLiteral("kcal"), 10000, 0, 1,
              QStringLiteral("orange"));
    m_calories->setDecimals(0);
    addMetric(QStringLiteral("蛋白质"), &m_protein, QStringLiteral("g"), 1000, 0, 2,
              QStringLiteral("lime"));
    addMetric(QStringLiteral("碳水"), &m_carbs, QStringLiteral("g"), 1000, 1, 0,
              QStringLiteral("blue"));
    addMetric(QStringLiteral("脂肪"), &m_fat, QStringLiteral("g"), 1000, 1, 1,
              QStringLiteral("coral"));
    numbers->setColumnStretch(0, 1);
    numbers->setColumnStretch(1, 1);
    numbers->setColumnStretch(2, 1);
    resultLayout->addLayout(numbers);

    auto *notesTitle = new QLabel(QStringLiteral("识别说明与估算依据"), resultPanel);
    notesTitle->setObjectName(QStringLiteral("FoodVisionFieldLabel"));
    resultLayout->addWidget(notesTitle);
    m_notes = new QPlainTextEdit(resultPanel);
    m_notes->setObjectName(QStringLiteral("AiNotes"));
    m_notes->setPlaceholderText(QStringLiteral("AI的估算依据会显示在这里，也可补充备注。"));
    m_notes->setFixedHeight(74);
    resultLayout->addWidget(m_notes);
    m_status = new QLabel(QStringLiteral("请先选择图片。AI估算并非称量或医学检测结果。"), resultPanel);
    m_status->setObjectName(QStringLiteral("AiStatusText"));
    m_status->setWordWrap(true);
    resultLayout->addWidget(m_status);
    content->addWidget(imagePanel);
    content->addWidget(resultPanel, 1);
    root->addLayout(content, 1);

    auto *footer = new QHBoxLayout;
    m_todaySummary = new QLabel(this);
    m_todaySummary->setObjectName(QStringLiteral("AiTodaySummary"));
    m_save = new QPushButton(QStringLiteral("保存到饮食记录"), this);
    m_save->setObjectName(QStringLiteral("FoodVisionSaveButton"));
    m_save->setFixedSize(220, 46);
    m_save->setEnabled(false);
    UiAssets::setButtonIcon(m_save, QStringLiteral("check"), 18,
                            QColor(QStringLiteral("#FFFFFF")));
    footer->addWidget(m_todaySummary);
    footer->addStretch();
    footer->addWidget(m_save);
    root->addLayout(footer);

    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    connect(chooseTop, &QPushButton::clicked, this, [this]() { chooseImage(false); });
    connect(chooseSide, &QPushButton::clicked, this, [this]() { chooseImage(true); });
    connect(m_analyze, &QPushButton::clicked, this, &FoodVisionDialog::analyze);
    connect(m_cancelRequest, &QPushButton::clicked, m_service, &NutritionAiService::cancel);
    connect(m_save, &QPushButton::clicked, this, &FoodVisionDialog::saveLog);
    connect(m_service, &NutritionAiService::busyChanged, this, &FoodVisionDialog::setBusy);
    connect(m_service, &NutritionAiService::foodAnalysisFinished,
            this, &FoodVisionDialog::applyResult);
    connect(m_itemSelector, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index) {
        if (m_loadingItem || index < 0)
            return;
        commitCurrentItem();
        m_currentItem = index;
        loadItem(index);
    });
    refreshTodaySummary();
}

void FoodVisionDialog::setReviewState(const QString &imagePath)
{
    if (!imagePath.isEmpty())
        setImagePath(imagePath, false);
    if (!imagePath.isEmpty())
        setImagePath(imagePath, true);
    FoodVisionResult result;
    result.ok = true;
    result.items = {
        {QStringLiteral("醋溜茄子"), QStringLiteral("菜品"), QStringLiteral("素菜"),
         QStringLiteral("酸香软嫩"), QStringLiteral("家常热菜"),
         QStringLiteral("含膳食纤维和钾"), 250.0, 286.0, 4.2, 31.0, 17.0,
         0.85, QStringLiteral("按一盘醋溜茄子和少量烹调油估算。"), {}}
    };
    result.foodName = QStringLiteral("醋溜茄子");
    result.itemType = QStringLiteral("菜品");
    result.category = QStringLiteral("素菜");
    result.servingGrams = 250.0;
    result.calories = 286.0;
    result.protein = 4.2;
    result.carbs = 31.0;
    result.fat = 17.0;
    result.confidence = 0.85;
    result.summary = QStringLiteral("整盘按一道成菜识别，不把茄子、辣椒和葱拆成独立食材。");
    result.assumptions = {QStringLiteral("按常见家用餐盘约250g估算"),
                          QStringLiteral("含少量烹调油和糖")};
    result.provider = QStringLiteral("review");
    result.items[0].servingMinGrams = 220.0;
    result.items[0].servingMaxGrams = 280.0;
    result.items[0].calibrationBasis = QStringLiteral("俯视图 + 侧视图 + 23厘米餐盘");
    result.servingMinGrams = 220.0;
    result.servingMaxGrams = 280.0;
    result.calibrationBasis = result.items[0].calibrationBasis;
    applyResult(result);
}

QDoubleSpinBox *FoodVisionDialog::createNumberField(const QString &unit, double maximum,
                                                     int decimals)
{
    auto *field = new QDoubleSpinBox(this);
    field->setObjectName(QStringLiteral("AiNumberInput"));
    field->setRange(0.0, maximum);
    field->setDecimals(decimals);
    field->setButtonSymbols(QAbstractSpinBox::NoButtons);
    field->setFixedHeight(38);
    UiAssets::attachFixedUnit(field, unit);
    return field;
}

void FoodVisionDialog::chooseImage(bool sideView)
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择餐食照片"), {},
        QStringLiteral("图片文件 (*.jpg *.jpeg *.png *.webp *.bmp)"));
    if (!path.isEmpty())
        setImagePath(path, sideView);
}

void FoodVisionDialog::setImagePath(const QString &path, bool sideView)
{
    const QPixmap original(path);
    if (original.isNull()) {
        QMessageBox::warning(this, QStringLiteral("无法读取"),
                             QStringLiteral("所选文件不是可识别的图片。"));
        return;
    }
    QLabel *preview = sideView ? m_sidePreview : m_topPreview;
    if (sideView)
        m_sideImagePath = path;
    else
        m_topImagePath = path;
    preview->setText({});
    preview->setPixmap(original.scaled(preview->size(), Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation));
    preview->setToolTip(QFileInfo(path).fileName());
    m_analyze->setEnabled(!m_topImagePath.isEmpty());
    m_save->setEnabled(false);
    m_status->setText(QStringLiteral("图片已就绪，点击“开始AI识别”。"));
    m_confidence->setText(QStringLiteral("未识别"));
}

void FoodVisionDialog::analyze()
{
    if (m_topImagePath.isEmpty())
        return;
    m_save->setEnabled(false);
    m_status->setText(QStringLiteral("正在识别食物并估算整份营养，请稍候…"));
    QStringList images{m_topImagePath};
    if (!m_sideImagePath.isEmpty())
        images.append(m_sideImagePath);
    const QString calibration = QStringLiteral("%1；%2")
        .arg(images.size() > 1 ? QStringLiteral("含俯视图和侧视图")
                               : QStringLiteral("仅俯视图"),
             m_reference ? m_reference->currentText() : QStringLiteral("无参照物"));
    m_service->analyzeFoodImages(images, calibration);
}

void FoodVisionDialog::applyResult(const FoodVisionResult &result)
{
    if (!result.ok) {
        m_status->setText(QStringLiteral("识别失败：%1").arg(result.error));
        m_confidence->setText(QStringLiteral("失败"));
        m_save->setEnabled(false);
        return;
    }
    m_provider = result.provider;
    m_confidenceValue = result.confidence;
    m_items = result.items;
    if (m_items.isEmpty()) {
        FoodVisionItem item;
        item.foodName = result.foodName;
        item.itemType = result.itemType;
        item.category = result.category;
        item.servingGrams = result.servingGrams;
        item.calories = result.calories;
        item.protein = result.protein;
        item.carbs = result.carbs;
        item.fat = result.fat;
        item.confidence = result.confidence;
        item.summary = result.summary;
        item.assumptions = result.assumptions;
        item.servingMinGrams = result.servingMinGrams;
        item.servingMaxGrams = result.servingMaxGrams;
        item.calibrationBasis = result.calibrationBasis;
        m_items.append(item);
    }
    m_itemNotes.clear();
    m_loadingItem = true;
    m_itemSelector->clear();
    for (int i = 0; i < m_items.size(); ++i) {
        const FoodVisionItem &item = m_items.at(i);
        m_itemSelector->addItem(QStringLiteral("%1/%2  %3")
                                    .arg(i + 1).arg(m_items.size()).arg(item.foodName));
        QStringList notes;
        if (!item.summary.isEmpty())
            notes.append(item.summary);
        for (const QString &assumption : item.assumptions)
            notes.append(QStringLiteral("• %1").arg(assumption));
        if (!item.calibrationBasis.isEmpty())
            notes.append(QStringLiteral("• 校准依据：%1").arg(item.calibrationBasis));
        m_itemNotes.append(notes.join(QLatin1Char('\n')));
    }
    m_itemSelector->setEnabled(m_items.size() > 1);
    m_itemSelector->setCurrentIndex(0);
    m_currentItem = 0;
    m_loadingItem = false;
    loadItem(0);
    m_status->setText(QStringLiteral("识别完成，共 %1 项。可逐项切换核对，确认后一次保存。")
                          .arg(m_items.size()));
    m_save->setEnabled(m_userId > 0);
}

void FoodVisionDialog::commitCurrentItem()
{
    if (m_loadingItem || m_currentItem < 0 || m_currentItem >= m_items.size())
        return;
    FoodVisionItem &item = m_items[m_currentItem];
    item.foodName = m_name->text().trimmed();
    item.servingGrams = m_grams->value();
    item.servingMinGrams = qMin(item.servingMinGrams, item.servingGrams);
    item.servingMaxGrams = qMax(item.servingMaxGrams, item.servingGrams);
    item.calories = m_calories->value();
    item.protein = m_protein->value();
    item.carbs = m_carbs->value();
    item.fat = m_fat->value();
    if (m_currentItem < m_itemNotes.size())
        m_itemNotes[m_currentItem] = m_notes->toPlainText().trimmed();
    m_itemSelector->setItemText(m_currentItem, QStringLiteral("%1/%2  %3")
        .arg(m_currentItem + 1).arg(m_items.size()).arg(item.foodName));
}

void FoodVisionDialog::loadItem(int index)
{
    if (index < 0 || index >= m_items.size())
        return;
    m_loadingItem = true;
    const FoodVisionItem &item = m_items.at(index);
    m_name->setText(item.foodName);
    m_grams->setValue(item.servingGrams);
    m_calories->setValue(item.calories);
    m_protein->setValue(item.protein);
    m_carbs->setValue(item.carbs);
    m_fat->setValue(item.fat);
    m_notes->setPlainText(m_itemNotes.value(index));
    m_confidence->setText(QStringLiteral("置信度 %1%").arg(qRound(item.confidence * 100.0)));
    m_rangeLabel->setText(QStringLiteral("估算 %1–%2 g · 推荐值 %3 g · %4")
                              .arg(qRound(item.servingMinGrams))
                              .arg(qRound(item.servingMaxGrams))
                              .arg(qRound(item.servingGrams))
                              .arg(item.calibrationBasis));
    m_confidenceValue = item.confidence;
    m_loadingItem = false;
}

void FoodVisionDialog::saveLog()
{
    commitCurrentItem();
    int saved = 0;
    for (int i = 0; i < m_items.size(); ++i) {
        const FoodVisionItem &item = m_items.at(i);
        FoodLogEntry entry;
        entry.userId = m_userId;
        entry.eatenAt = QDateTime::currentDateTime();
        entry.mealLabel = m_meal->currentText();
        entry.foodName = item.foodName.trimmed();
        entry.servingGrams = item.servingGrams;
        entry.calories = item.calories;
        entry.protein = item.protein;
        entry.carbs = item.carbs;
        entry.fat = item.fat;
        entry.confidence = item.confidence;
        entry.provider = m_provider;
        entry.notes = m_itemNotes.value(i);
        QString error;
        // 同一张照片只存一份副本，其余记录通过同一次识别建立。
        const int logId = FoodLogDAO().create(entry, i == 0 ? m_topImagePath : QString(), &error);
        if (logId <= 0) {
            QMessageBox::warning(this, QStringLiteral("保存失败"),
                                 QStringLiteral("已保存 %1 项，第 %2 项失败：%3")
                                     .arg(saved).arg(i + 1).arg(error));
            return;
        }
        ++saved;
        emit foodLogSaved(logId);
    }
    m_save->setEnabled(false);
    m_status->setText(QStringLiteral("已自动写入 %1 项饮食记录，可继续识别下一餐。")
                          .arg(saved));
    refreshTodaySummary();
}

void FoodVisionDialog::setBusy(bool busy)
{
    m_analyze->setEnabled(!busy && !m_topImagePath.isEmpty());
    m_cancelRequest->setVisible(busy);
}

void FoodVisionDialog::refreshTodaySummary()
{
    const DailyFoodLogTotals totals = FoodLogDAO().totalsForDate(m_userId, QDate::currentDate());
    m_todaySummary->setText(totals.count > 0
        ? QStringLiteral("今日已记录 %1 项 · %2 kcal").arg(totals.count).arg(qRound(totals.calories))
        : QStringLiteral("今日尚无拍照饮食记录"));
}

void FoodVisionDialog::closeEvent(QCloseEvent *event)
{
    m_service->cancel();
    QDialog::closeEvent(event);
}
