#include "HealthSyncDialog.h"

#include "UiAssets.h"
#include "../dao/HealthDataDAO.h"
#include "../services/HealthDataSyncService.h"

#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace {
QFrame *makeSourceCard(QWidget *parent, const QString &icon, const QString &title,
                       const QString &subtitle, QLabel **statusOut,
                       QLabel **detailOut, QPushButton **buttonOut)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("HealthSourceCard"));
    auto *layout = new QHBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(14);

    auto *iconLabel = UiAssets::createIconLabel(card, icon, 30,
                                                QColor(QStringLiteral("#059669")));
    iconLabel->setObjectName(QStringLiteral("HealthSourceIcon"));
    iconLabel->setFixedSize(52, 52);
    auto *copy = new QVBoxLayout;
    copy->setSpacing(3);
    auto *name = new QLabel(title, card);
    name->setObjectName(QStringLiteral("HealthSourceTitle"));
    auto *description = new QLabel(subtitle, card);
    description->setObjectName(QStringLiteral("HealthSourceSubtitle"));
    description->setWordWrap(true);
    auto *detail = new QLabel(QStringLiteral("尚未同步"), card);
    detail->setObjectName(QStringLiteral("HealthSourceDetail"));
    detail->setWordWrap(true);
    copy->addWidget(name);
    copy->addWidget(description);
    copy->addWidget(detail);

    auto *right = new QVBoxLayout;
    right->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto *status = new QLabel(QStringLiteral("待连接"), card);
    status->setObjectName(QStringLiteral("HealthStatusBadge"));
    status->setAlignment(Qt::AlignCenter);
    status->setFixedSize(84, 30);
    auto *button = new QPushButton(QStringLiteral("导入同步包"), card);
    button->setObjectName(QStringLiteral("HealthImportButton"));
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedSize(132, 38);
    UiAssets::setButtonIcon(button, QStringLiteral("refresh"), 17,
                            QColor(QStringLiteral("#FFFFFF")));
    right->addWidget(status, 0, Qt::AlignRight);
    right->addWidget(button, 0, Qt::AlignRight);

    layout->addWidget(iconLabel, 0, Qt::AlignTop);
    layout->addLayout(copy, 1);
    layout->addLayout(right);
    *statusOut = status;
    *detailOut = detail;
    *buttonOut = button;
    return card;
}
}

HealthSyncDialog::HealthSyncDialog(int userId, QWidget *parent)
    : QDialog(parent), m_userId(userId)
{
    setObjectName(QStringLiteral("HealthSyncDialog"));
    setWindowTitle(QStringLiteral("健康数据同步"));
    setFixedSize(820, 610);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 22);
    root->setSpacing(14);

    auto *header = new QHBoxLayout;
    auto *titles = new QVBoxLayout;
    titles->setSpacing(3);
    auto *title = new QLabel(QStringLiteral("连接健康数据"), this);
    title->setObjectName(QStringLiteral("HealthSyncTitle"));
    auto *subtitle = new QLabel(
        QStringLiteral("汇总步数、活动消耗、体重与睡眠，让每日目标响应真实变化。"), this);
    subtitle->setObjectName(QStringLiteral("HealthSyncSubtitle"));
    auto *close = new QPushButton(this);
    close->setObjectName(QStringLiteral("AiCloseButton"));
    close->setFixedSize(36, 36);
    UiAssets::setButtonIcon(close, QStringLiteral("close"), 20);
    titles->addWidget(title);
    titles->addWidget(subtitle);
    header->addLayout(titles, 1);
    header->addWidget(close, 0, Qt::AlignTop);
    root->addLayout(header);

    auto *privacy = new QFrame(this);
    privacy->setObjectName(QStringLiteral("HealthPrivacyCard"));
    auto *privacyLayout = new QHBoxLayout(privacy);
    privacyLayout->setContentsMargins(14, 11, 14, 11);
    auto *privacyIcon = UiAssets::createIconLabel(privacy, QStringLiteral("shield"), 20,
                                                  QColor(QStringLiteral("#0891B2")));
    auto *privacyText = new QLabel(
        QStringLiteral("隐私说明：HealthKit 与 Health Connect 均须在手机端由你授权。"
                       "当前桌面端读取授权后导出的同步包，不会声称绕过系统权限直连。"), privacy);
    privacyText->setObjectName(QStringLiteral("HealthPrivacyText"));
    privacyText->setWordWrap(true);
    privacyLayout->addWidget(privacyIcon, 0, Qt::AlignTop);
    privacyLayout->addWidget(privacyText, 1);
    root->addWidget(privacy);

    QPushButton *appleButton = nullptr;
    QPushButton *androidButton = nullptr;
    root->addWidget(makeSourceCard(
        this, QStringLiteral("apple"), QStringLiteral("Apple 健康 · HealthKit"),
        QStringLiteral("支持 Apple 健康导出 XML；读取活动、体重和睡眠记录。"),
        &m_appleStatus, &m_appleDetail, &appleButton));
    root->addWidget(makeSourceCard(
        this, QStringLiteral("medical-heart"), QStringLiteral("Android · Health Connect"),
        QStringLiteral("支持 Health Connect JSON 或标准桥接 CSV；按日期合并四类指标。"),
        &m_androidStatus, &m_androidDetail, &androidButton));

    auto *formatHint = new QLabel(
        QStringLiteral("通用 CSV 字段：date, steps, active_calories, weight_kg, sleep_hours。"
                       "系统只保存每日汇总，不保存原始定位或运动轨迹。"), this);
    formatHint->setObjectName(QStringLiteral("HealthFormatHint"));
    formatHint->setWordWrap(true);
    root->addWidget(formatHint);

    m_result = new QLabel(QStringLiteral("导入后，饮食分析页会立即重新计算 7 / 14 / 30 天动态目标。"), this);
    m_result->setObjectName(QStringLiteral("HealthImportResult"));
    m_result->setWordWrap(true);
    m_result->setMinimumHeight(54);
    root->addWidget(m_result);
    root->addStretch();

    connect(close, &QPushButton::clicked, this, &HealthSyncDialog::reject);
    connect(appleButton, &QPushButton::clicked, this, [this]() {
        importPlatform(QStringLiteral("apple_health"),
                       QStringLiteral("Apple 健康导出 (*.xml);;标准健康数据 (*.csv)"));
    });
    connect(androidButton, &QPushButton::clicked, this, [this]() {
        importPlatform(QStringLiteral("health_connect"),
                       QStringLiteral("Health Connect 数据 (*.json *.csv)"));
    });
    refreshStatuses();
}

void HealthSyncDialog::importPlatform(const QString &platform, const QString &fileFilter)
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择健康数据同步包"),
                                                      QString(), fileFilter);
    if (path.isEmpty())
        return;
    const HealthImportResult result = HealthDataSyncService().importFile(m_userId, platform, path);
    if (!result.ok) {
        m_result->setProperty("state", QStringLiteral("error"));
        m_result->setText(QStringLiteral("导入失败：%1").arg(result.error));
        style()->unpolish(m_result);
        style()->polish(m_result);
        return;
    }
    m_result->setProperty("state", QStringLiteral("success"));
    m_result->setText(QStringLiteral("同步完成：%1，共 %2 天（%3 至 %4）。动态目标已重新计算。")
                          .arg(result.message)
                          .arg(result.importedDays)
                          .arg(result.fromDate.toString(QStringLiteral("yyyy-MM-dd")))
                          .arg(result.toDate.toString(QStringLiteral("yyyy-MM-dd"))));
    style()->unpolish(m_result);
    style()->polish(m_result);
    refreshStatuses();
    emit dataImported();
}

void HealthSyncDialog::refreshStatuses()
{
    updateSourceCard(QStringLiteral("apple_health"), QStringLiteral("待连接"),
                     QStringLiteral("尚未同步；请从手机端导出 Apple 健康数据。"));
    updateSourceCard(QStringLiteral("health_connect"), QStringLiteral("待连接"),
                     QStringLiteral("尚未同步；请从 Android 端导出 Health Connect 数据。"));
    for (const HealthSourceStatus &source : HealthDataDAO().sourceStatuses(m_userId)) {
        const QString detail = QStringLiteral("最近同步 %1 · %2 天 · %3 至 %4")
            .arg(source.lastSyncedAt.toString(QStringLiteral("M/d HH:mm")))
            .arg(source.recordCount)
            .arg(source.fromDate.toString(QStringLiteral("M/d")))
            .arg(source.toDate.toString(QStringLiteral("M/d")));
        updateSourceCard(source.platform, QStringLiteral("已同步"), detail);
    }
}

void HealthSyncDialog::updateSourceCard(const QString &platform, const QString &status,
                                        const QString &detail)
{
    QLabel *badge = platform.startsWith(QLatin1String("apple"))
        ? m_appleStatus : m_androidStatus;
    QLabel *copy = platform.startsWith(QLatin1String("apple"))
        ? m_appleDetail : m_androidDetail;
    if (!badge || !copy)
        return;
    badge->setText(status);
    badge->setProperty("connected", status == QStringLiteral("已同步"));
    copy->setText(detail);
    badge->style()->unpolish(badge);
    badge->style()->polish(badge);
}

void HealthSyncDialog::setReviewState()
{
    updateSourceCard(QStringLiteral("apple_health"), QStringLiteral("已同步"),
                     QStringLiteral("最近同步 09:42 · 14 天 · 8/19 至 9/1"));
    updateSourceCard(QStringLiteral("health_connect"), QStringLiteral("待连接"),
                     QStringLiteral("可继续连接另一台 Android 设备，重复日期不会重复计数。"));
    m_result->setProperty("state", QStringLiteral("success"));
    m_result->setText(QStringLiteral("已读取 14 天数据：日均 8420 步、活动消耗 430 kcal、睡眠 7.1 小时；体重下降 0.4 kg。"));
    style()->unpolish(m_result);
    style()->polish(m_result);
}
