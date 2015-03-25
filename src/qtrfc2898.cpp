/* Copyright 2013 Naikel Aparicio. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ''AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL EELI REILIN OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the author and should not be interpreted as representing
 * official policies, either expressed or implied, of the copyright holder.
 */

#include "qtrfc2898.h"
#include "protocolexception.h"

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#include <QMessageAuthenticationCode>
#else
#include <QCryptographicHash>
// Code taken from http://qt-project.org/wiki/HMAC-SHA1
QByteArray hash(QByteArray baseString,QByteArray key,QCryptographicHash::Algorithm al)
{
    int blockSize = 64; // HMAC-SHA-1 block size, defined in SHA-1 standard
    if (key.length() > blockSize) { // if key is longer than block size (64), reduce key length with SHA-1 compression
        key = QCryptographicHash::hash(key, al);
    }

    QByteArray innerPadding(blockSize, char(0x36)); // initialize inner padding with char "6"
    QByteArray outerPadding(blockSize, char(0x5c)); // initialize outer padding with char "\"
    // ascii characters 0x36 ("6") and 0x5c ("\") are selected because they have large
    // Hamming distance (http://en.wikipedia.org/wiki/Hamming_distance)

    for (int i = 0; i < key.length(); i++) {
        innerPadding[i] = innerPadding[i] ^ key.at(i); // XOR operation between every byte in key and innerpadding, of key length
        outerPadding[i] = outerPadding[i] ^ key.at(i); // XOR operation between every byte in key and outerpadding, of key length
    }

    // result = hash ( outerPadding CONCAT hash ( innerPadding CONCAT baseString ) ).toBase64
    QByteArray total = outerPadding;
    QByteArray part = innerPadding;
    part.append(baseString);
    total.append(QCryptographicHash::hash(part, al));
    QByteArray hashed = QCryptographicHash::hash(total, al);
    return hashed.toBase64();
}
#endif

#define SHA1_DIGEST_LENGTH          20

QtRFC2898::QtRFC2898()
{
}

QByteArray QtRFC2898::deriveBytes(const QByteArray& password, const QByteArray& salt, int iterations)
{
    if (iterations == 0)
    {
        throw ProtocolException("PBKDF2: Invalid iteration count");
    }

    if (password.length() == 0)
    {
        throw ProtocolException("PBKDF2: Empty password is invalid");
    }

    QByteArray obuf, output;
    QByteArray asalt = salt;
    asalt.resize(salt.length() + 4);

    QByteArray key = password;
    int key_len = ( key.length() > SHA1_DIGEST_LENGTH ) ? SHA1_DIGEST_LENGTH : key.length();

    for (int count = 1; count < key_len && output.size() < key_len; count ++)
    {
        asalt[salt.length() + 0] = (count >> 24) & 0xff;
        asalt[salt.length() + 1] = (count >> 16) & 0xff;
        asalt[salt.length() + 2] = (count >> 8) & 0xff;
        asalt[salt.length() + 3] = count & 0xff;

        QByteArray d1 =
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
                QMessageAuthenticationCode::
#else
                hash(asalt, key, QCryptographicHash::Sha1);
#endif
        obuf = d1;
        char *obuf_data = obuf.data();

        for (int i = 1; i < iterations; i++)
        {
            QByteArray d2 =
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
                    QMessageAuthenticationCode::
#else
                    hash(d1, key, QCryptographicHash::Sha1);
#endif
            d1 = d2;
            for (int j = 0; j < obuf.length(); j++)
                obuf_data[j] ^= d1.at(j);
        }

        output.append(obuf);
    }

    return output.left(key_len);
}
