#ifndef FRIDGEITEM_H
#define FRIDGEITEM_H

#include <QString>

struct FridgeItem
{
    int id = 0;
    int userId = 0;
    QString foodName;
    double quantity = 1.0;
    QString unit; // 可选：个 / g / 份
    QString expiryDate; // ISO yyyy-MM-dd；为空表示未设置
};

#endif // FRIDGEITEM_H
