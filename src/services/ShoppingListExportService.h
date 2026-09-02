#ifndef SHOPPINGLISTEXPORTSERVICE_H
#define SHOPPINGLISTEXPORTSERVICE_H

#include "ShoppingListService.h"

class ShoppingListExportService
{
public:
    enum class Format { Pdf, Word, Excel, Text };

    static QString extension(Format format);
    static bool exportList(const QString &path, Format format,
                           const QList<ShoppingListItem> &items,
                           const QString &scopeLabel, QString *error = nullptr);
};

#endif // SHOPPINGLISTEXPORTSERVICE_H
