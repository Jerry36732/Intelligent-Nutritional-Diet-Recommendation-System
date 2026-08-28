#ifndef AUTHUTILS_H
#define AUTHUTILS_H

#include <QString>

namespace AuthUtils {
/** 生成 salt:sha256(salt+password) */
QString hashPassword(const QString &password, const QString &salt = QStringLiteral("smartdiet"));
bool verifyPassword(const QString &password, const QString &storedHash);
}

#endif // AUTHUTILS_H
