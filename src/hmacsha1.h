#ifndef HMACSHA1_H
#define HMACSHA1_H

#include <QObject>
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#include <QMessageAuthenticationCode>
#endif
#include <QCryptographicHash>

class HmacSha1
{
public:
    HmacSha1(const QByteArray &key);

    QByteArray hmacSha1(const QByteArray &buffer);

private:
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    QMessageAuthenticationCode *m_mac;
#else
    QByteArray m_key;
#endif

};

#endif // HMACSHA1_H
