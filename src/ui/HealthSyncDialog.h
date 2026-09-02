#ifndef HEALTHSYNCDIALOG_H
#define HEALTHSYNCDIALOG_H

#include <QDialog>

class QLabel;

class HealthSyncDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HealthSyncDialog(int userId, QWidget *parent = nullptr);

    void setReviewState();

signals:
    void dataImported();

private:
    void importPlatform(const QString &platform, const QString &fileFilter);
    void refreshStatuses();
    void updateSourceCard(const QString &platform, const QString &status,
                          const QString &detail);

    int m_userId = 0;
    QLabel *m_appleStatus = nullptr;
    QLabel *m_appleDetail = nullptr;
    QLabel *m_androidStatus = nullptr;
    QLabel *m_androidDetail = nullptr;
    QLabel *m_result = nullptr;
};

#endif // HEALTHSYNCDIALOG_H
