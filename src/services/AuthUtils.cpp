#include "AuthUtils.h"

#include <QCryptographicHash>

namespace AuthUtils {

QString hashPassword(const QString &password, const QString &salt)
{
    const QByteArray data = (salt + password).toUtf8();
    const QByteArray digest = QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
    return salt + QLatin1Char(':') + QString::fromLatin1(digest);
}

bool verifyPassword(const QString &password, const QString &storedHash)
{
    if (storedHash.isEmpty())
        return false;
    const int sep = storedHash.indexOf(QLatin1Char(':'));
    if (sep <= 0)
        return false;
    const QString salt = storedHash.left(sep);
    return hashPassword(password, salt) == storedHash;
}

} // namespace AuthUtils
