#include "hmacsha1.h"

HmacSha1::HmacSha1(const QByteArray &key)
{
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    m_mac = new QMessageAuthenticationCode(QCryptographicHash::Sha1, key);
#else
    m_key = key;
#endif
}

QByteArray HmacSha1::hmacSha1(const QByteArray &buffer)
{
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    m_mac->reset();
    m_mac->addData(buffer);
    return m_mac->result();
#else
    int blockSize = 64; // HMAC-SHA-1 block size, defined in SHA-1 standard
    if (m_key.length() > blockSize) { // if key is longer than block size (64), reduce key length with SHA-1 compression
        m_key = QCryptographicHash::hash(m_key, QCryptographicHash::Sha1);
    }

    QByteArray innerPadding(blockSize, char(0x36)); // initialize inner padding with char "6"
    QByteArray outerPadding(blockSize, char(0x5c)); // initialize outer padding with char "\"
    // ascii characters 0x36 ("6") and 0x5c ("\") are selected because they have large
    // Hamming distance (http://en.wikipedia.org/wiki/Hamming_distance)

    for (int i = 0; i < m_key.length(); i++) {
        innerPadding[i] = innerPadding[i] ^ m_key.at(i); // XOR operation between every byte in key and innerpadding, of key length
        outerPadding[i] = outerPadding[i] ^ m_key.at(i); // XOR operation between every byte in key and outerpadding, of key length
    }

    // result = hash ( outerPadding CONCAT hash ( innerPadding CONCAT baseString ) ).toBase64
    QByteArray total = outerPadding;
    QByteArray part = innerPadding;
    part.append(buffer);
    total.append(QCryptographicHash::hash(part, QCryptographicHash::Sha1));
    QByteArray hashed = QCryptographicHash::hash(total, QCryptographicHash::Sha1);
    return hashed;
#endif
}
