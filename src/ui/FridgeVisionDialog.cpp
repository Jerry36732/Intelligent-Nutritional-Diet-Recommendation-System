#include "FridgeVisionDialog.h"
#include "UiAssets.h"

#include "../dao/FridgeDAO.h"

#include <QDate>
#include <QDateEdit>
#include <QComboBox>
#include <QCloseEvent>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QString friendlyVisionError(const QString &detail)
{
    if (detail.contains(QStringLiteral("JSON"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("unterminated"), Qt::CaseInsensitive)
        || detail.contains(QStringLiteral("array"), Qt::CaseInsensitive)) {
        return QStringLiteral("AI 返回内容不完整，请重新识别。建议让食材分开放置，并保持画面明亮清晰。");
    }
    if (detail.contains(QStringLiteral("云端")) || detail.contains(QStringLiteral("网络"))
        || detail.contains(QStringLiteral("API"), Qt::CaseInsensitive)) {
        return QStringLiteral("暂时无法连接识别服务，请检查网络后重新选择照片识别。");
    }
    return QStringLiteral("暂时没有识别出可用食材，请调整拍摄角度后重试。");
}
} // namespace

FridgeVisionDialog *FridgeVisionDialog::create(int userId, QWidget *parent)
{
    // 在与构造函数相同的编译单元分配对象，避免增量编译时调用方沿用旧类大小。
    return new FridgeVisionDialog(userId, parent);
}

FridgeVisionDialog::FridgeVisionDialog(int userId, QWidget *parent)
    : QDialog(parent)
    , m_userId(userId)
    , m_service(new NutritionAiService(this))
{
    setWindowTitle(QStringLiteral("拍照添加食材"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setObjectName(QStringLiteral("FridgeVisionDialog"));
    setModal(true);
    setFixedSize(900, 650);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(32, 24, 32, 26);
    root->setSpacing(20);

    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("拍照添加食材"), this);
    title->setObjectName(QStringLiteral("FridgeVisionTitle"));
    title->setFont(UiAssets::titleFont(29));
    auto *close = new QPushButton(this);
    close->setObjectName(QStringLiteral("DialogCloseButton"));
    close->setFixedSize(38, 38);
    UiAssets::setButtonIcon(close, QStringLiteral("close"), 18);
    header->addWidget(title, 1);
    header->addWidget(close, 0, Qt::AlignTop);
    root->addLayout(header);

    auto *content = new QHBoxLayout;
    content->setSpacing(24);
    auto *photoCard = new QFrame(this);
    photoCard->setObjectName(QStringLiteral("FridgeVisionPhotoCard"));
    photoCard->setFixedWidth(390);
    auto *photoLay = new QVBoxLayout(photoCard);
    photoLay->setContentsMargins(15, 15, 15, 15);
    photoLay->setSpacing(14);
    m_preview = new QLabel(photoCard);
    m_preview->setObjectName(QStringLiteral("FridgeVisionPreview"));
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumSize(360, 430);
    m_preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_preview->setWordWrap(true);
    m_preview->setTextFormat(Qt::RichText);
    m_preview->setText(QStringLiteral(
        "<div align='center'><img src=':/icons/v5/camera.svg' width='58' height='58'><br><br>"
        "<b>选择一张清晰的食材照片</b><br>"
        "<span style='color:#64748B'>支持 JPG、PNG、WebP</span></div>"));
    m_choose = new QPushButton(QStringLiteral("选择照片并识别"), photoCard);
    m_choose->setObjectName(QStringLiteral("AiPrimaryButton"));
    m_choose->setMinimumHeight(50);
    UiAssets::setButtonIcon(m_choose, QStringLiteral("camera"), 18, QColor(Qt::white));
    photoLay->addWidget(m_preview, 1);
    photoLay->addWidget(m_choose);

    auto *formCard = new QFrame(this);
    formCard->setObjectName(QStringLiteral("FridgeVisionResultCard"));
    auto *form = new QVBoxLayout(formCard);
    form->setContentsMargins(18, 16, 18, 16);
    form->setSpacing(9);
    auto *resultTitle = new QLabel(QStringLiteral("识别结果"), formCard);
    resultTitle->setObjectName(QStringLiteral("FridgeVisionSectionTitle"));
    m_confidence = new QLabel(QStringLiteral("等待识别"), formCard);
    m_confidence->setObjectName(QStringLiteral("VisionConfidenceBadge"));
    auto *resultHeader = new QHBoxLayout;
    resultHeader->addWidget(resultTitle);
    resultHeader->addStretch();
    resultHeader->addWidget(m_confidence);
    form->addLayout(resultHeader);

    m_itemSelector = new QComboBox(formCard);
    m_itemSelector->setObjectName(QStringLiteral("FridgeVisionInput"));
    m_itemSelector->addItem(QStringLiteral("识别后将自动填入，支持逐项核对"));
    m_itemSelector->setEnabled(false);
    form->addWidget(m_itemSelector);

    auto addLabel = [formCard, form](const QString &text) {
        auto *label = new QLabel(text, formCard);
        label->setObjectName(QStringLiteral("AiFieldLabel"));
        form->addWidget(label);
    };
    addLabel(QStringLiteral("食材名称"));
    m_name = new QLineEdit(formCard);
    m_name->setObjectName(QStringLiteral("FridgeVisionInput"));
    m_name->setPlaceholderText(QStringLiteral("识别后可手动修正"));
    form->addWidget(m_name);
    addLabel(QStringLiteral("估算重量（可拖动调整）"));
    auto *weightRow = new QHBoxLayout;
    m_weightSlider = new QSlider(Qt::Horizontal, formCard);
    m_weightSlider->setObjectName(QStringLiteral("FridgeVisionWeightSlider"));
    m_weightSlider->setRange(1, 3000);
    m_weightSlider->setSingleStep(5);
    m_weightSlider->setPageStep(50);
    m_weightSlider->setValue(100);
    m_weightSpin = new QDoubleSpinBox(formCard);
    m_weightSpin->setObjectName(QStringLiteral("FridgeVisionWeightSpin"));
    m_weightSpin->setRange(1.0, 3000.0);
    m_weightSpin->setDecimals(0);
    m_weightSpin->setSuffix(QStringLiteral(" g"));
    m_weightSpin->setValue(100.0);
    m_weightSpin->setFixedWidth(102);
    weightRow->addWidget(m_weightSlider, 1);
    weightRow->addWidget(m_weightSpin);
    form->addLayout(weightRow);
    addLabel(QStringLiteral("保质期（由你确认）"));
    m_expiry = new QDateEdit(QDate::currentDate().addDays(7), formCard);
    m_expiry->setObjectName(QStringLiteral("FridgeVisionInput"));
    m_expiry->setCalendarPopup(true);
    m_expiry->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_expiry->setMinimumDate(QDate::currentDate());
    form->addWidget(m_expiry);
    m_details = new QLabel(QStringLiteral("识别结果仅用于快速录入，重量会受拍摄角度和参照物影响。"), formCard);
    m_details->setObjectName(QStringLiteral("FridgeVisionDetails"));
    m_details->setWordWrap(true);
    form->addWidget(m_details, 1);
    m_status = new QLabel(QStringLiteral("照片只在识别请求中使用，不写入冰箱数据库。"), formCard);
    m_status->setObjectName(QStringLiteral("FridgeVisionStatus"));
    m_status->setWordWrap(true);
    m_next = new QPushButton(QStringLiteral("下一项"), formCard);
    m_next->setObjectName(QStringLiteral("FridgeVisionNextButton"));
    m_next->setMinimumHeight(46);
    m_next->setCursor(Qt::PointingHandCursor);
    m_next->setVisible(false);
    UiAssets::setButtonIcon(m_next, QStringLiteral("chevron-right"), 18,
                            QColor(QStringLiteral("#047857")));
    m_save = new QPushButton(QStringLiteral("添加到冰箱"), formCard);
    m_save->setObjectName(QStringLiteral("AiPrimaryButton"));
    m_save->setMinimumHeight(46);
    m_save->setEnabled(false);
    UiAssets::setButtonIcon(m_save, QStringLiteral("plus"), 18, QColor(Qt::white));
    form->addWidget(m_status);
    auto *actionRow = new QHBoxLayout;
    actionRow->setSpacing(10);
    actionRow->addWidget(m_next);
    actionRow->addWidget(m_save, 1);
    form->addLayout(actionRow);

    content->addWidget(photoCard);
    content->addWidget(formCard, 1);
    root->addLayout(content, 1);

    connect(close, &QPushButton::clicked, this, &FridgeVisionDialog::reject);
    connect(m_choose, &QPushButton::clicked, this, &FridgeVisionDialog::chooseImage);
    connect(m_save, &QPushButton::clicked, this, &FridgeVisionDialog::saveIngredient);
    connect(m_next, &QPushButton::clicked, this, &FridgeVisionDialog::nextItem);
    connect(m_service, &NutritionAiService::busyChanged, this, &FridgeVisionDialog::setBusy);
    connect(m_service, &NutritionAiService::foodAnalysisFinished,
            this, &FridgeVisionDialog::applyResult);
    connect(m_itemSelector, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index) {
        if (m_loadingItem || index < 0)
            return;
        commitCurrentItem();
        m_currentItem = index;
        loadItem(index);
    });
    connect(m_weightSlider, &QSlider::valueChanged, this,
            [this](int value) { m_weightSpin->setValue(value); });
    connect(m_weightSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double value) { m_weightSlider->setValue(qRound(value)); });
    setResultControlsEnabled(false);
}

FridgeVisionDialog::~FridgeVisionDialog()
{
    if (m_service)
        m_service->cancel();
}

void FridgeVisionDialog::reject()
{
    if (m_service)
        m_service->cancel();
    QDialog::reject();
}

void FridgeVisionDialog::closeEvent(QCloseEvent *event)
{
    if (m_service)
        m_service->cancel();
    QDialog::closeEvent(event);
}

void FridgeVisionDialog::chooseImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择食材照片"), {},
        QStringLiteral("图片文件 (*.jpg *.jpeg *.png *.webp *.bmp)"));
    if (path.isEmpty())
        return;
    m_imagePath = path;
    showImage(path);
    m_items.clear();
    m_expiries.clear();
    m_currentItem = -1;
    m_itemSelector->clear();
    m_itemSelector->addItem(QStringLiteral("正在识别图片中的食材…"));
    setResultControlsEnabled(false);
    m_next->setVisible(false);
    m_save->setEnabled(false);
    m_confidence->setText(QStringLiteral("识别中"));
    m_status->setText(QStringLiteral("正在识别食材并估算重量，请稍候…"));
    m_service->analyzeIngredientImage(path);
}

void FridgeVisionDialog::showImage(const QString &path)
{
    const QPixmap image(path);
    if (image.isNull())
        return;
    QTimer::singleShot(0, this, [this, image]() {
        const QSize area = m_preview->contentsRect().size();
        if (area.isValid()) {
            m_preview->setPixmap(image.scaled(area, Qt::KeepAspectRatio,
                                              Qt::SmoothTransformation));
        }
    });
}

void FridgeVisionDialog::applyResult(const FoodVisionResult &result)
{
    if (!result.ok) {
        m_items.clear();
        m_expiries.clear();
        m_currentItem = -1;
        m_loadingItem = true;
        m_itemSelector->clear();
        m_itemSelector->addItem(QStringLiteral("识别失败，请重新选择照片"));
        m_itemSelector->setEnabled(false);
        m_name->clear();
        m_weightSlider->setValue(100);
        m_weightSpin->setValue(100.0);
        m_loadingItem = false;
        setResultControlsEnabled(false);
        m_details->setText(friendlyVisionError(result.error));
        m_details->setToolTip(result.error);
        m_status->setText(QStringLiteral("本次未添加任何食材。可点击左侧按钮重新选择照片识别。"));
        m_confidence->setText(QStringLiteral("识别失败"));
        m_save->setEnabled(false);
        m_next->setVisible(false);
        m_choose->setText(QStringLiteral("重新选择照片并识别"));
        return;
    }
    m_details->setToolTip({});
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
        m_items.append(item);
    }
    m_expiries.fill(QDate::currentDate().addDays(7), m_items.size());
    m_loadingItem = true;
    m_itemSelector->clear();
    for (int i = 0; i < m_items.size(); ++i) {
        m_itemSelector->addItem(QStringLiteral("%1/%2  %3")
                                    .arg(i + 1).arg(m_items.size())
                                    .arg(m_items.at(i).foodName));
    }
    m_itemSelector->setEnabled(m_items.size() > 1);
    m_itemSelector->setCurrentIndex(0);
    m_currentItem = 0;
    m_loadingItem = false;
    setResultControlsEnabled(true);
    loadItem(0);
    m_status->setText(QStringLiteral("识别到 %1 种食材。请逐项核对重量和保质期后一次添加。")
                          .arg(m_items.size()));
    m_save->setEnabled(m_userId > 0);
    m_next->setVisible(m_items.size() > 1);
    updateNextButton();
}

void FridgeVisionDialog::commitCurrentItem()
{
    if (m_loadingItem || m_currentItem < 0 || m_currentItem >= m_items.size())
        return;
    FoodVisionItem &item = m_items[m_currentItem];
    item.foodName = m_name->text().trimmed();
    item.servingGrams = m_weightSpin->value();
    if (m_currentItem < m_expiries.size())
        m_expiries[m_currentItem] = m_expiry->date();
    m_itemSelector->setItemText(m_currentItem, QStringLiteral("%1/%2  %3")
        .arg(m_currentItem + 1).arg(m_items.size()).arg(item.foodName));
}

void FridgeVisionDialog::loadItem(int index)
{
    if (index < 0 || index >= m_items.size())
        return;
    m_loadingItem = true;
    const FoodVisionItem &item = m_items.at(index);
    const int grams = qBound(1, qRound(item.servingGrams), 3000);
    m_name->setText(item.foodName);
    m_weightSlider->setValue(grams);
    m_weightSpin->setValue(grams);
    m_expiry->setDate(m_expiries.value(index, QDate::currentDate().addDays(7)));
    m_confidence->setText(QStringLiteral("置信度 %1%").arg(qRound(item.confidence * 100.0)));
    QString details = QStringLiteral("AI 估算：%1 g · %2 kcal").arg(grams).arg(qRound(item.calories));
    if (!item.summary.isEmpty())
        details += QStringLiteral("\n%1").arg(item.summary);
    if (!item.assumptions.isEmpty())
        details += QStringLiteral("\n估算依据：%1").arg(item.assumptions.join(QStringLiteral("；")));
    m_details->setText(details);
    m_loadingItem = false;
    updateNextButton();
}

void FridgeVisionDialog::setResultControlsEnabled(bool enabled)
{
    m_name->setEnabled(enabled);
    m_weightSlider->setEnabled(enabled);
    m_weightSpin->setEnabled(enabled);
    m_expiry->setEnabled(enabled);
    m_itemSelector->setEnabled(enabled && m_items.size() > 1);
}

void FridgeVisionDialog::updateNextButton()
{
    if (!m_next)
        return;
    const bool hasNext = m_currentItem >= 0 && m_currentItem + 1 < m_items.size();
    m_next->setEnabled(hasNext);
    m_next->setText(hasNext
        ? QStringLiteral("下一项（%1/%2）").arg(m_currentItem + 2).arg(m_items.size())
        : QStringLiteral("已到最后一项"));
    UiAssets::setButtonIcon(m_next, QStringLiteral("chevron-right"), 18,
                            QColor(hasNext ? QStringLiteral("#047857")
                                           : QStringLiteral("#8AA096")));
}

void FridgeVisionDialog::nextItem()
{
    if (m_currentItem < 0 || m_currentItem + 1 >= m_items.size())
        return;
    m_itemSelector->setCurrentIndex(m_currentItem + 1);
}

void FridgeVisionDialog::setBusy(bool busy)
{
    m_choose->setEnabled(!busy);
    if (busy)
        m_choose->setText(QStringLiteral("正在识别…"));
    else {
        m_choose->setText(QStringLiteral("重新选择照片"));
        UiAssets::setButtonIcon(m_choose, QStringLiteral("camera"), 18, QColor(Qt::white));
    }
}

void FridgeVisionDialog::saveIngredient()
{
    commitCurrentItem();
    if (m_userId <= 0 || m_items.isEmpty()) {
        m_status->setText(QStringLiteral("请先登录，并确认食材名称。"));
        return;
    }
    int saved = 0;
    for (int i = 0; i < m_items.size(); ++i) {
        const FoodVisionItem &item = m_items.at(i);
        if (item.foodName.trimmed().isEmpty() || item.servingGrams <= 0.0)
            continue;
        if (!FridgeDAO().upsert(m_userId, item.foodName, item.servingGrams,
                                QStringLiteral("g"),
                                m_expiries.value(i, QDate::currentDate().addDays(7))
                                    .toString(Qt::ISODate))) {
            QMessageBox::warning(this, QStringLiteral("添加失败"),
                                 QStringLiteral("第 %1 项“%2”写入冰箱库存失败。")
                                     .arg(i + 1).arg(item.foodName));
            return;
        }
        ++saved;
    }
    if (saved == 0) {
        m_status->setText(QStringLiteral("请确认至少一项有效食材名称和重量。"));
        return;
    }
    emit ingredientAdded();
    accept();
}

void FridgeVisionDialog::setReviewState(const QString &imagePath)
{
    if (!imagePath.isEmpty()) {
        m_imagePath = imagePath;
        showImage(imagePath);
    }
    FoodVisionResult result;
    result.ok = true;
    result.items = {
        {QStringLiteral("三文鱼"), QStringLiteral("食材"), QStringLiteral("鱼虾海鲜"),
         {}, {}, QStringLiteral("富含优质蛋白和不饱和脂肪"), 250.0, 520.0,
         50.0, 0.0, 32.0, 0.90, QStringLiteral("按两块鱼排估算。"), {}},
        {QStringLiteral("鸡蛋"), QStringLiteral("食材"), QStringLiteral("肉禽蛋"),
         {}, {}, QStringLiteral("含优质蛋白"), 180.0, 259.0, 22.7, 2.0, 18.0,
         0.88, QStringLiteral("按约3枚鸡蛋估算。"), {}},
        {QStringLiteral("菠菜"), QStringLiteral("食材"), QStringLiteral("蔬菜"),
         {}, {}, QStringLiteral("含叶酸和膳食纤维"), 220.0, 62.0, 6.3, 10.0, 0.9,
         0.84, QStringLiteral("按一把可食部分估算。"), {}}
    };
    result.foodName = QStringLiteral("三文鱼、鸡蛋、菠菜");
    result.servingGrams = 650.0;
    result.calories = 841.0;
    result.confidence = 0.87;
    applyResult(result);
}

void FridgeVisionDialog::setFailureReviewState()
{
    FoodVisionResult result;
    result.error = QStringLiteral("云端失败：识别结果不是合法 JSON：unterminated array；本地失败：模型未返回识别内容");
    applyResult(result);
}
