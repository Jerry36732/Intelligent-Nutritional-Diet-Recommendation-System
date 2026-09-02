#include "FoodLogDAO.h"

#include "DatabaseManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <QtGlobal>

namespace {
FoodLogEntry rowToEntry(const QSqlQuery &query)
{
    FoodLogEntry entry;
    entry.id = query.value(QStringLiteral("id")).toInt();
    entry.userId = query.value(QStringLiteral("user_id")).toInt();
    entry.eatenAt = QDateTime::fromString(query.value(QStringLiteral("eaten_at")).toString(),
                                         Qt::ISODate);
    entry.mealLabel = query.value(QStringLiteral("meal_label")).toString();
    entry.foodName = query.value(QStringLiteral("food_name")).toString();
    entry.servingGrams = query.value(QStringLiteral("serving_grams")).toDouble();
    entry.calories = query.value(QStringLiteral("calories")).toDouble();
    entry.protein = query.value(QStringLiteral("protein")).toDouble();
    entry.carbs = query.value(QStringLiteral("carbs")).toDouble();
    entry.fat = query.value(QStringLiteral("fat")).toDouble();
    entry.confidence = query.value(QStringLiteral("confidence")).toDouble();
    entry.provider = query.value(QStringLiteral("provider")).toString();
    entry.imagePath = query.value(QStringLiteral("image_path")).toString();
    entry.notes = query.value(QStringLiteral("notes")).toString();
    return entry;
}
} // namespace

QString FoodLogDAO::persistImage(int userId, const QString &sourcePath,
                                 QString *errorMessage) const
{
    if (sourcePath.trimmed().isEmpty())
        return {};
    const QFileInfo source(sourcePath);
    if (!source.exists() || !source.isFile()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("原始图片不存在，营养记录未保存。");
        return {};
    }
    QString suffix = source.suffix().toLower();
    if (suffix.isEmpty())
        suffix = QStringLiteral("jpg");
    const QString databasePath = DatabaseManager::getInstance().database().databaseName();
    const QString storageRoot = !databasePath.isEmpty() && databasePath != QLatin1String(":memory:")
        ? QFileInfo(databasePath).absolutePath()
        : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QString directoryPath = storageRoot
                                  + QStringLiteral("/food_log_images/%1").arg(userId);
    QDir directory;
    if (!directory.mkpath(directoryPath)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法创建饮食记录图片目录。");
        return {};
    }
    const QString destination = directoryPath + QLatin1Char('/')
        + QUuid::createUuid().toString(QUuid::WithoutBraces) + QLatin1Char('.') + suffix;
    if (!QFile::copy(source.absoluteFilePath(), destination)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法复制饮食记录图片。");
        return {};
    }
    return QDir::toNativeSeparators(destination);
}

int FoodLogDAO::create(FoodLogEntry entry, const QString &sourceImagePath,
                       QString *errorMessage) const
{
    auto fail = [&](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return 0;
    };
    if (entry.userId <= 0 || entry.foodName.trimmed().isEmpty()
        || entry.servingGrams <= 0.0 || entry.calories < 0.0)
        return fail(QStringLiteral("用户、食物名称、份量或热量无效。"));
    QSqlDatabase database = DatabaseManager::getInstance().database();
    if (!database.isOpen())
        return fail(QStringLiteral("数据库不可用。"));

    QString imageError;
    const QString imagePath = persistImage(entry.userId, sourceImagePath, &imageError);
    if (!sourceImagePath.isEmpty() && imagePath.isEmpty())
        return fail(imageError);

    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO user_food_logs(user_id,eaten_at,meal_label,food_name,serving_grams,"
        "calories,protein,carbs,fat,confidence,provider,image_path,notes) "
        "VALUES(:user,:eaten,:meal,:name,:grams,:calories,:protein,:carbs,:fat,"
        ":confidence,:provider,:image,:notes)"));
    query.bindValue(QStringLiteral(":user"), entry.userId);
    query.bindValue(QStringLiteral(":eaten"),
                    (entry.eatenAt.isValid() ? entry.eatenAt : QDateTime::currentDateTime())
                        .toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":meal"), entry.mealLabel.trimmed());
    query.bindValue(QStringLiteral(":name"), entry.foodName.trimmed());
    query.bindValue(QStringLiteral(":grams"), entry.servingGrams);
    query.bindValue(QStringLiteral(":calories"), entry.calories);
    query.bindValue(QStringLiteral(":protein"), qMax(0.0, entry.protein));
    query.bindValue(QStringLiteral(":carbs"), qMax(0.0, entry.carbs));
    query.bindValue(QStringLiteral(":fat"), qMax(0.0, entry.fat));
    query.bindValue(QStringLiteral(":confidence"), qBound(0.0, entry.confidence, 1.0));
    query.bindValue(QStringLiteral(":provider"), entry.provider.trimmed());
    query.bindValue(QStringLiteral(":image"), imagePath);
    query.bindValue(QStringLiteral(":notes"), entry.notes.trimmed());
    if (!query.exec()) {
        if (!imagePath.isEmpty())
            QFile::remove(imagePath);
        return fail(query.lastError().text());
    }
    return query.lastInsertId().toInt();
}

bool FoodLogDAO::remove(int id, int userId, QString *errorMessage) const
{
    auto fail = [&](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };
    if (id <= 0 || userId <= 0)
        return fail(QStringLiteral("饮食记录或用户无效。"));

    QSqlDatabase database = DatabaseManager::getInstance().database();
    if (!database.isOpen())
        return fail(QStringLiteral("数据库不可用。"));

    QSqlQuery lookup(database);
    lookup.prepare(QStringLiteral(
        "SELECT image_path FROM user_food_logs WHERE id=:id AND user_id=:user"));
    lookup.bindValue(QStringLiteral(":id"), id);
    lookup.bindValue(QStringLiteral(":user"), userId);
    if (!lookup.exec())
        return fail(lookup.lastError().text());
    if (!lookup.next())
        return fail(QStringLiteral("记录不存在或不属于当前用户。"));
    const QString imagePath = lookup.value(0).toString();
    lookup.finish();

    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "DELETE FROM user_food_logs WHERE id=:id AND user_id=:user"));
    query.bindValue(QStringLiteral(":id"), id);
    query.bindValue(QStringLiteral(":user"), userId);
    if (!query.exec())
        return fail(query.lastError().text());
    if (query.numRowsAffected() != 1)
        return fail(QStringLiteral("饮食记录未被删除，请刷新后重试。"));

    if (!imagePath.isEmpty())
        QFile::remove(imagePath);
    return true;
}

QList<FoodLogEntry> FoodLogDAO::recentByUser(int userId, int limit) const
{
    QList<FoodLogEntry> result;
    QSqlQuery query(DatabaseManager::getInstance().database());
    query.prepare(QStringLiteral(
        "SELECT * FROM user_food_logs WHERE user_id=:user "
        "ORDER BY eaten_at DESC,id DESC LIMIT :limit"));
    query.bindValue(QStringLiteral(":user"), userId);
    query.bindValue(QStringLiteral(":limit"), qBound(1, limit, 100));
    if (!query.exec())
        return result;
    while (query.next())
        result.append(rowToEntry(query));
    return result;
}

DailyFoodLogTotals FoodLogDAO::totalsForDate(int userId, const QDate &date) const
{
    DailyFoodLogTotals totals;
    QSqlQuery query(DatabaseManager::getInstance().database());
    query.prepare(QStringLiteral(
        "SELECT COUNT(*) AS item_count,COALESCE(SUM(calories),0),COALESCE(SUM(protein),0),"
        "COALESCE(SUM(carbs),0),COALESCE(SUM(fat),0) FROM user_food_logs "
        "WHERE user_id=:user AND date(eaten_at)=:day"));
    query.bindValue(QStringLiteral(":user"), userId);
    query.bindValue(QStringLiteral(":day"), date.toString(Qt::ISODate));
    if (query.exec() && query.next()) {
        totals.count = query.value(0).toInt();
        totals.calories = query.value(1).toDouble();
        totals.protein = query.value(2).toDouble();
        totals.carbs = query.value(3).toDouble();
        totals.fat = query.value(4).toDouble();
    }
    return totals;
}

QList<DailyFoodLogPoint> FoodLogDAO::dailyTotals(int userId, const QDate &from,
                                                 const QDate &to) const
{
    QList<DailyFoodLogPoint> result;
    if (userId <= 0 || !from.isValid() || !to.isValid() || from > to)
        return result;

    QHash<QDate, DailyFoodLogTotals> byDate;
    QSqlQuery query(DatabaseManager::getInstance().database());
    query.prepare(QStringLiteral(
        "SELECT date(eaten_at),COUNT(*),COALESCE(SUM(calories),0),"
        "COALESCE(SUM(protein),0),COALESCE(SUM(carbs),0),COALESCE(SUM(fat),0) "
        "FROM user_food_logs WHERE user_id=:user AND date(eaten_at)>=:from "
        "AND date(eaten_at)<=:to GROUP BY date(eaten_at) ORDER BY date(eaten_at)"));
    query.bindValue(QStringLiteral(":user"), userId);
    query.bindValue(QStringLiteral(":from"), from.toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":to"), to.toString(Qt::ISODate));
    if (query.exec()) {
        while (query.next()) {
            const QDate date = QDate::fromString(query.value(0).toString(), Qt::ISODate);
            DailyFoodLogTotals totals;
            totals.count = query.value(1).toInt();
            totals.calories = query.value(2).toDouble();
            totals.protein = query.value(3).toDouble();
            totals.carbs = query.value(4).toDouble();
            totals.fat = query.value(5).toDouble();
            if (date.isValid())
                byDate.insert(date, totals);
        }
    }

    for (QDate date = from; date <= to; date = date.addDays(1))
        result.append({date, byDate.value(date)});
    return result;
}
