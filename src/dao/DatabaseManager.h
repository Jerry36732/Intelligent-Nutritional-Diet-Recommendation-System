#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QString>

class DatabaseManager
{
public:
    static DatabaseManager &getInstance();

    bool open(const QString &path);
    QSqlDatabase database() const;
    bool ensureSchema();
    bool isOpen() const;

    DatabaseManager(const DatabaseManager &) = delete;
    DatabaseManager &operator=(const DatabaseManager &) = delete;

private:
    DatabaseManager() = default;
    ~DatabaseManager() = default;

    static constexpr const char *kConnectionName = "smart_diet";
};

#endif // DATABASEMANAGER_H
