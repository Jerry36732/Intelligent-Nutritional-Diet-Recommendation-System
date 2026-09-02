#ifndef WEBRECIPEIMPORTSERVICE_H
#define WEBRECIPEIMPORTSERVICE_H

#include <QByteArray>
#include <QString>
#include <QStringList>

struct WebRecipeImportResult
{
    enum class State {
        Complete,
        Incomplete,
        LoginRequired,
        VerificationRequired,
        AccessBlocked,
        InvalidContent,
    };

    State state = State::Incomplete;
    QString name;
    QStringList ingredients;
    QStringList steps;
    QString category;
    int minutes = 20;
    QString message;

    bool isComplete() const { return state == State::Complete; }
    bool isRestricted() const
    {
        return state == State::LoginRequired || state == State::VerificationRequired
               || state == State::AccessBlocked;
    }
};

class WebRecipeImportService
{
public:
    static WebRecipeImportResult parseHtml(const QByteArray &htmlBytes,
                                           const QString &finalUrl = QString(),
                                           int httpStatus = 200,
                                           const QString &contentType = QString());
    static WebRecipeImportResult parseCopiedText(const QString &text);
};

#endif // WEBRECIPEIMPORTSERVICE_H
