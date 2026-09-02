#include "IngredientVisionDialog.h"

#include "RecipeDetailDialog.h"
#include "UiAssets.h"
#include "../dao/RecipeDAO.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QFrame *makeInfoCard(QWidget *parent, const QString &title, const QString &tone,
                     QLabel **body)
{
    auto *card = new QFrame(parent);
    card->setProperty("class", QStringLiteral("IngredientVisionInfoCard"));
    card->setProperty("tone", tone);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 9, 12, 9);
    layout->setSpacing(4);
    auto *caption = new QLabel(title, card);
    caption->setObjectName(QStringLiteral("IngredientVisionInfoTitle"));
    *body = new QLabel(QStringLiteral("等待识别后显示"), card);
    (*body)->setObjectName(QStringLiteral("IngredientVisionInfoBody"));
    (*body)->setWordWrap(true);
    (*body)->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(caption);
    layout->addWidget(*body, 1);
    return card;
}

FoodVisionItem firstResultItem(const FoodVisionResult &result)
{
    if (!result.items.isEmpty())
        return result.items.first();
    return FoodVisionItem{result.foodName, result.itemType, result.category,
                          result.taste, result.commonUses, result.nutritionHighlights,
                          result.servingGrams, result.calories, result.protein,
                          result.carbs, result.fat, result.confidence,
                          result.summary, result.assumptions};
}
} // namespace

IngredientVisionDialog::IngredientVisionDialog(int userId, QWidget *parent)
    : QDialog(parent)
    , m_userId(userId)
    , m_service(new NutritionAiService(this))
{
    setWindowTitle(QStringLiteral("食材识别"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setObjectName(QStringLiteral("IngredientVisionDialog"));
    setModal(true);
    setFixedSize(900, 660);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(30, 24, 30, 26);
    root->setSpacing(18);

    auto *header = new QHBoxLayout;
    auto *titleBox = new QVBoxLayout;
    titleBox->setSpacing(3);
    auto *title = new QLabel(QStringLiteral("食材识别"), this);
    title->setObjectName(QStringLiteral("IngredientVisionTitle"));
    title->setFont(UiAssets::titleFont(29));
    auto *subtitle = new QLabel(
        QStringLiteral("识别食材名称、特点、常见用途，并从食谱库匹配可用菜谱。"), this);
    subtitle->setObjectName(QStringLiteral("IngredientVisionSubtitle"));
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);
    auto *close = new QPushButton(this);
    close->setObjectName(QStringLiteral("DialogCloseButton"));
    close->setFixedSize(38, 38);
    UiAssets::setButtonIcon(close, QStringLiteral("close"), 19);
    header->addLayout(titleBox, 1);
    header->addWidget(close, 0, Qt::AlignTop);
    root->addLayout(header);

    auto *content = new QHBoxLayout;
    content->setSpacing(22);

    auto *photoCard = new QFrame(this);
    photoCard->setObjectName(QStringLiteral("IngredientVisionPhotoCard"));
    photoCard->setFixedWidth(350);
    auto *photoLayout = new QVBoxLayout(photoCard);
    photoLayout->setContentsMargins(15, 15, 15, 15);
    photoLayout->setSpacing(13);
    m_preview = new QLabel(photoCard);
    m_preview->setObjectName(QStringLiteral("IngredientVisionPreview"));
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setWordWrap(true);
    m_preview->setTextFormat(Qt::RichText);
    m_preview->setText(QStringLiteral(
        "<div align='center'><img src=':/icons/v5/camera.svg' width='52' height='52'><br><br>"
        "<b>选择一张清晰的食材照片</b><br><span style='color:#64748B'>支持 JPG、PNG、WebP</span></div>"));
    m_preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_choose = new QPushButton(QStringLiteral("选择照片并识别"), photoCard);
    m_choose->setObjectName(QStringLiteral("IngredientVisionChooseButton"));
    m_choose->setMinimumHeight(50);
    m_choose->setCursor(Qt::PointingHandCursor);
    UiAssets::setButtonIcon(m_choose, QStringLiteral("camera"), 19, Qt::white);
    photoLayout->addWidget(m_preview, 1);
    photoLayout->addWidget(m_choose);

    auto *resultCard = new QFrame(this);
    resultCard->setObjectName(QStringLiteral("IngredientVisionResultCard"));
    auto *resultLayout = new QVBoxLayout(resultCard);
    resultLayout->setContentsMargins(18, 16, 18, 16);
    resultLayout->setSpacing(10);

    auto *resultHeader = new QHBoxLayout;
    auto *resultTitle = new QLabel(QStringLiteral("识别结果"), resultCard);
    resultTitle->setObjectName(QStringLiteral("IngredientVisionSectionTitle"));
    m_confidence = new QLabel(QStringLiteral("等待识别"), resultCard);
    m_confidence->setObjectName(QStringLiteral("IngredientVisionConfidence"));
    resultHeader->addWidget(resultTitle);
    resultHeader->addStretch();
    resultHeader->addWidget(m_confidence);
    resultLayout->addLayout(resultHeader);

    m_selector = new QComboBox(resultCard);
    m_selector->setObjectName(QStringLiteral("IngredientVisionInput"));
    m_selector->addItem(QStringLiteral("识别后可在此切换不同食材"));
    m_selector->setEnabled(false);
    resultLayout->addWidget(m_selector);

    auto *identityRow = new QHBoxLayout;
    identityRow->setSpacing(10);
    m_name = new QLineEdit(resultCard);
    m_name->setObjectName(QStringLiteral("IngredientVisionInput"));
    m_name->setPlaceholderText(QStringLiteral("识别后显示食材名称"));
    m_name->setReadOnly(true);
    m_category = new QLabel(QStringLiteral("类别待识别"), resultCard);
    m_category->setObjectName(QStringLiteral("IngredientVisionCategory"));
    identityRow->addWidget(m_name, 1);
    identityRow->addWidget(m_category);
    resultLayout->addLayout(identityRow);

    auto *infoGrid = new QGridLayout;
    infoGrid->setHorizontalSpacing(9);
    infoGrid->setVerticalSpacing(9);
    infoGrid->addWidget(makeInfoCard(resultCard, QStringLiteral("味道与特点"),
                                     QStringLiteral("green"), &m_taste), 0, 0);
    infoGrid->addWidget(makeInfoCard(resultCard, QStringLiteral("常见作用与用途"),
                                     QStringLiteral("blue"), &m_uses), 0, 1);
    infoGrid->addWidget(makeInfoCard(resultCard, QStringLiteral("主要营养特点"),
                                     QStringLiteral("orange"), &m_nutrition), 1, 0, 1, 2);
    infoGrid->setColumnStretch(0, 1);
    infoGrid->setColumnStretch(1, 1);
    resultLayout->addLayout(infoGrid);

    auto *recipeTitle = new QLabel(QStringLiteral("可用菜谱"), resultCard);
    recipeTitle->setObjectName(QStringLiteral("IngredientVisionRecipeTitle"));
    m_recipes = new QListWidget(resultCard);
    m_recipes->setObjectName(QStringLiteral("IngredientVisionRecipeList"));
    m_recipes->setFixedHeight(112);
    m_recipes->addItem(QStringLiteral("识别完成后将从食谱大全匹配菜谱"));
    resultLayout->addWidget(recipeTitle);
    resultLayout->addWidget(m_recipes);

    m_status = new QLabel(QStringLiteral("图片仅用于本次识别，不会写入饮食记录或冰箱库存。"), resultCard);
    m_status->setObjectName(QStringLiteral("IngredientVisionStatus"));
    m_status->setWordWrap(true);
    resultLayout->addWidget(m_status);

    content->addWidget(photoCard);
    content->addWidget(resultCard, 1);
    root->addLayout(content, 1);

    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_choose, &QPushButton::clicked, this, &IngredientVisionDialog::chooseImage);
    connect(m_service, &NutritionAiService::busyChanged,
            this, &IngredientVisionDialog::setBusy);
    connect(m_service, &NutritionAiService::foodAnalysisFinished,
            this, &IngredientVisionDialog::applyResult);
    connect(m_selector, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &IngredientVisionDialog::loadItem);
    connect(m_recipes, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem *) { openSelectedRecipe(); });
}

void IngredientVisionDialog::chooseImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择食材照片"), {},
        QStringLiteral("图片文件 (*.jpg *.jpeg *.png *.webp *.bmp)"));
    if (path.isEmpty())
        return;
    m_imagePath = path;
    showImage(path);
    m_confidence->setText(QStringLiteral("识别中"));
    m_status->setText(QStringLiteral("正在识别食材名称、特点和用途，请稍候…"));
    m_service->analyzeIngredientImage(path);
}

void IngredientVisionDialog::showImage(const QString &path)
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

void IngredientVisionDialog::applyResult(const FoodVisionResult &result)
{
    if (!result.ok) {
        m_items.clear();
        m_confidence->setText(QStringLiteral("识别失败"));
        m_status->setText(QStringLiteral("识别失败：%1").arg(result.error));
        return;
    }
    m_items = result.items;
    if (m_items.isEmpty())
        m_items.append(firstResultItem(result));
    m_selector->blockSignals(true);
    m_selector->clear();
    for (int i = 0; i < m_items.size(); ++i) {
        m_selector->addItem(QStringLiteral("%1/%2  %3")
                                .arg(i + 1).arg(m_items.size()).arg(m_items.at(i).foodName));
    }
    m_selector->setEnabled(m_items.size() > 1);
    m_selector->blockSignals(false);
    loadItem(0);
    m_status->setText(QStringLiteral("识别到 %1 种食材；双击菜谱名称可查看完整做法。")
                          .arg(m_items.size()));
}

void IngredientVisionDialog::setBusy(bool busy)
{
    m_choose->setEnabled(!busy);
    m_choose->setText(busy ? QStringLiteral("正在识别…")
                           : QStringLiteral("重新选择照片"));
    if (!busy)
        UiAssets::setButtonIcon(m_choose, QStringLiteral("camera"), 19, Qt::white);
}

void IngredientVisionDialog::loadItem(int index)
{
    if (index < 0 || index >= m_items.size())
        return;
    const FoodVisionItem &item = m_items.at(index);
    m_name->setText(item.foodName);
    m_category->setText(item.category.isEmpty() ? QStringLiteral("其他") : item.category);
    m_taste->setText(item.taste.isEmpty() ? QStringLiteral("图片中无法准确判断") : item.taste);
    m_uses->setText(item.commonUses.isEmpty() ? QStringLiteral("可用于日常烹饪搭配")
                                              : item.commonUses);
    m_nutrition->setText(item.nutritionHighlights.isEmpty()
                             ? QStringLiteral("营养特点需结合具体品种和重量判断")
                             : item.nutritionHighlights);
    m_confidence->setText(QStringLiteral("置信度 %1%")
                              .arg(qRound(item.confidence * 100.0)));
    refreshRecipes(item.foodName);
}

void IngredientVisionDialog::refreshRecipes(const QString &ingredientName)
{
    m_recipes->clear();
    m_recipeIds.clear();
    int total = 0;
    QList<Recipe> recipes = RecipeDAO().browse(ingredientName, QStringLiteral("all"),
                                                m_userId, 0, 6, &total);
    if (recipes.isEmpty() && ingredientName.size() > 2)
        recipes = RecipeDAO().browse(ingredientName.left(2), QStringLiteral("all"),
                                     m_userId, 0, 6, &total);
    for (const Recipe &recipe : recipes) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1  ·  %2 kcal  ·  %3分钟")
                .arg(recipe.name).arg(qRound(recipe.totalCalories)).arg(recipe.cookMinutes),
            m_recipes);
        item->setData(Qt::UserRole, recipe.id);
        m_recipeIds.append(recipe.id);
    }
    if (recipes.isEmpty())
        m_recipes->addItem(QStringLiteral("暂未匹配到包含该食材的菜谱"));
}

void IngredientVisionDialog::openSelectedRecipe()
{
    const QListWidgetItem *item = m_recipes->currentItem();
    const int recipeId = item ? item->data(Qt::UserRole).toInt() : 0;
    if (recipeId <= 0)
        return;
    const Recipe recipe = RecipeDAO().findById(recipeId);
    if (recipe.isValid()) {
        RecipeDetailDialog dialog(recipe, m_userId, this);
        dialog.exec();
    }
}

void IngredientVisionDialog::setReviewState(const QString &imagePath)
{
    if (!imagePath.isEmpty()) {
        m_imagePath = imagePath;
        showImage(imagePath);
    }
    FoodVisionResult result;
    result.ok = true;
    result.items = {
        {QStringLiteral("茄子"), QStringLiteral("食材"), QStringLiteral("蔬菜"),
         QStringLiteral("口感软嫩，味道清淡，容易吸收酱汁"),
         QStringLiteral("常用于红烧、清炒、蒸制，也可搭配肉末和豆制品"),
         QStringLiteral("含膳食纤维、钾和花青素，烹调时注意控制用油"),
         250.0, 58.0, 2.8, 12.0, 0.5, 0.92,
         QStringLiteral("识别为紫皮茄子"), {}}
    };
    applyResult(result);
}

void IngredientVisionDialog::closeEvent(QCloseEvent *event)
{
    m_service->cancel();
    QDialog::closeEvent(event);
}
