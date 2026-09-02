#ifndef RECIPEEDITORDIALOG_H
#define RECIPEEDITORDIALOG_H

#include <QDialog>

class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QNetworkReply;
class QPlainTextEdit;
class QSpinBox;
class QPushButton;

class RecipeEditorDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode { Manual, WebImport };
    explicit RecipeEditorDialog(int userId, Mode mode, QWidget *parent = nullptr);

signals:
    void recipeCreated(int recipeId);

private slots:
    void fetchWebRecipe();
    void handleWebReply(QNetworkReply *reply);
    void parsePastedRecipe();
    void openWebRecipeInBrowser();
    void saveRecipe();

private:
    void applyImportResult(const struct WebRecipeImportResult &result);
    void showPasteFallback(bool visible);
    void setImportBusy(bool busy, const QString &message = QString());

    int m_userId = 0;
    Mode m_mode = Mode::Manual;
    QNetworkAccessManager *m_network = nullptr;
    QLineEdit *m_url = nullptr;
    QPushButton *m_fetch = nullptr;
    QLabel *m_importStatus = nullptr;
    QPushButton *m_fallbackToggle = nullptr;
    QFrame *m_pastePanel = nullptr;
    QPlainTextEdit *m_pastedRecipe = nullptr;
    QLineEdit *m_name = nullptr;
    QComboBox *m_category = nullptr;
    QSpinBox *m_minutes = nullptr;
    QPlainTextEdit *m_ingredients = nullptr;
    QPlainTextEdit *m_steps = nullptr;
};

#endif // RECIPEEDITORDIALOG_H
