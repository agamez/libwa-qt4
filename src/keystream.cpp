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

#include "keystream.h"
#include "qtrfc2898.h"

#include <QDebug>

KeyStream::KeyStream(QByteArray rc4key, QByteArray mackey, QObject *parent) : QObject(parent)
{
    rc4 = new RC4(rc4key, 0x300);
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    mac = new QMessageAuthenticationCode(QCryptographicHash::Sha1, mackey);
#else
    m_key = mackey;
#endif
    seq = 0;
}

bool KeyStream::decodeMessage(QByteArray& buffer, int macOffset, int offset, int length)
{
    //qDebug() << "decodeMessage seq:" << seq;
    QByteArray base = buffer.left(buffer.size() - 4);
    QByteArray hmac = buffer.right(4);
    QByteArray buffer2 = processBuffer(base, seq++);

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    buffer2 = hmacSha1(buffer2);
#else
    buffer2 = hmacSha1(m_key,buffer2);
#endif

    QByteArray origBuffer = buffer;
    buffer = base;
    rc4->Cipher(buffer.data(), 0, buffer.size());

    for (int i = 0; i < 4; i++)
    {
        if (buffer2[macOffset + i] != hmac[i])
        {
            qDebug() << "error decoding message. macOffset:" << macOffset << "offset:" << offset << "length:" << length << "bufferSize:" << buffer.size();
            qDebug() << "buffer mac:" << buffer2.toHex() << "hmac:" << hmac.toHex();
            qDebug() << "buffer:" << buffer.toHex();
            qDebug() << "origBuffer:" << origBuffer.toHex();
            return false;
        }
    }
    return true;
}


void KeyStream::encodeMessage(QByteArray &buffer, int macOffset, int offset, int length)
{
    //qDebug() << "encodeMessage seq:" << seq;
    rc4->Cipher(buffer.data(), offset, length);
    QByteArray base = buffer.mid(offset, length);
    base = processBuffer(base, seq++);

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    QByteArray hmac = hmacSha1(base).left(4);
#else
    QByteArray hmac = hmacSha1(m_key,base).left(4);
#endif

    buffer.replace(macOffset, 4, hmac.constData(), 4);
}

QList<QByteArray> KeyStream::keyFromPasswordAndNonce(const QByteArray& pass, const QByteArray& nonce)
{
    QList<QByteArray> keys;

    QtRFC2898 bytes;

    for (int i = 1; i < 5; i++) {
        QByteArray nnonce = nonce;
        nnonce.append(i);
        keys.append(bytes.deriveBytes(pass, nnonce, 2));
    }

    return keys;
}

QByteArray KeyStream::processBuffer(QByteArray buffer, int seq)
{
    buffer.append(seq >> 0x18);
    buffer.append(seq >> 0x10);
    buffer.append(seq >> 0x8);
    buffer.append(seq);
    return buffer;
}

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
QByteArray KeyStream::hmacSha1(const QByteArray &data)
{
    mac->reset();
    mac->addData(data);
    return mac->result();
}
#else
// Code taken from http://qt-project.org/wiki/HMAC-SHA1
QByteArray KeyStream::hmacSha1(QByteArray key, QByteArray baseString)
{
    int blockSize = 64; // HMAC-SHA-1 block size, defined in SHA-1 standard
    if (key.length() > blockSize) { // if key is longer than block size (64), reduce key length with SHA-1 compression
        key = QCryptographicHash::hash(key, QCryptographicHash::Sha1);
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
    total.append(QCryptographicHash::hash(part, QCryptographicHash::Sha1));
    QByteArray hashed = QCryptographicHash::hash(total, QCryptographicHash::Sha1);
    return hashed.toBase64();
}
#endif
