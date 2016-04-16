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

#include <QDataStream>

#include "attributelist.h"
#include "bintreenodereader.h"

#define READ_TIMEOUT 30000

BinTreeNodeReader::BinTreeNodeReader(QTcpSocket *socket, WATokenDictionary *dict,
                                     QObject *parent) : QObject(parent)
{
    this->dict = dict;
    this->socket = socket;

    reset();
}

void BinTreeNodeReader::reset()
{
    inputKey = NULL;

    if (decodedStream.isOpen())
        decodedStream.close();
    rawBuffer.clear();
    decodedBuffer.clear();
}

bool BinTreeNodeReader::getOneToplevelStream()
{
    qint32 bufferSize = readRawInt24();
    qint8 flags = (bufferSize & 0xf00000) >> 20;
    bufferSize &= 0x0fffff;

    if (!fillRawBuffer(bufferSize)) {
        return false;
    }

    //qDebug() << "[[ " + readBuffer.toHex();
    if (decodedStream.isOpen()) {
        decodedStream.close();
    }

    if (decodeRawStream(flags, 0, bufferSize)) {
        decodedStream.setBuffer(&decodedBuffer);
        decodedStream.open(QIODevice::ReadOnly);
    } else {
        return false;
    }
    return true;
}

int BinTreeNodeReader::getOneToplevelStreamSize() {
    return decodedBuffer.size()
            + 3 //sizeof(Int24) - buffer size
            + 4; //offset size (encoded length size)
}

bool BinTreeNodeReader::decodeRawStream(qint8 flags, qint32 offset, qint32 length)
{
    if ((flags & 8) != 0)
    {
        if (length < 4) {
            qDebug() << "Invalid length 0x" << QString::number(length,16);
            harakiri();
            return false;
        }

        offset += 4;
        length -= 4;
        if (!inputKey->decodeMessage(rawBuffer, offset - 4, offset, length)) {
            qDebug() << "error decoding message";
            harakiri();
            return false;
        }

        decodedBuffer = rawBuffer.right(length);
        //qDebug() << "<< " + decodedBuffer.toHex();
    } else {
        //copy as is
        decodedBuffer = rawBuffer;
    }
    rawBuffer.clear();
    return true;
}

bool BinTreeNodeReader::nextTree(ProtocolTreeNode& node)
{
    bool result;

    if (!getOneToplevelStream()) {
        return false;
    }

    node.setSize(getOneToplevelStreamSize());

    result = nextTreeInternal(node);
    qDebug() << "read" << node.getSize() << node.toString() << "\n" << result;
    return result;
}

bool BinTreeNodeReader::nextTreeInternal(ProtocolTreeNode& node)
{
    quint8 b;

    if (!readInt8(b)) {
        qDebug() << "Failed readInt8" << b;
        return false;
    }
    int size = -1;
    if (!readListSize(b, size)) {
        qDebug() << "Failed readListSize" << b;
        return false;
    }
    if (size < 0) {
        qDebug() << "size < 0";
        return false;
    }
    if (!readInt8(b)) {
        qDebug() << "Failed readInt8" << b;
        return false;
    }
    QByteArray tag;
    if (!readString(b, tag)) {
        qDebug() << "Failed readString" << b;
        return false;
    }
    int attribCount = (size - 2 + size % 2) / 2;

    AttributeList attribs;
    if (!readAttributes(attribs,attribCount)) {
        qDebug() << "Failed readAttributes" << attribCount;
        return false;
    }
    node.setTag(tag);
    node.setAttributes(attribs);

    if ((size % 2) == 1)
        return true;

    if (!readInt8(b)) {
        qDebug() << "Failed readInt8" << b;
        return false;
    }
    if (isListTag(b))
    {
        return readList(b, node);
    }

    QByteArray data;
    if (!readString(b, data)) {
        qDebug() << "Failed readString" << b;
        return false;
    }
    node.setData(data);
    return true;
}

bool BinTreeNodeReader::isListTag(quint32 b)
{
    return (b == 248) || (b == 0) || (b == 249);
}

bool BinTreeNodeReader::readList(qint32 token, ProtocolTreeNode& node)
{
    int size = 0;
    if (!readListSize(token, size)) {
        qDebug() << "failed to read listSize";
        return false;
    }
    for (int i = 0; i < size; i++)
    {
        ProtocolTreeNode child;
        //TODO: check results
        nextTreeInternal(child);
        node.addChild(child);
    }
    return true;
}

bool BinTreeNodeReader::readAttributes(AttributeList& attribs, int attribCount)
{
    QByteArray key, value;
    for (int i=0; i < attribCount; i++)
    {
        if (readString(key) && readString(value)) {
            attribs.insert(QString::fromUtf8(key), QString::fromUtf8(value));
        } else {
            qDebug() << "failed to read attribute key:value";
            //TODO: return false;
        }
    }
    return true;
}

bool BinTreeNodeReader::readListSize(qint32 token, int& size)
{
    size = -1;
    if (token == 0) {
        size = 0;
    } else if (token == 248) {
        quint8 b;
        if (!readInt8(b)) {
            qDebug() << "failed to read 8bit size";
            return false;
        }
        size = b;
    } else if (token == 249) {
        qint16 b; //TODO: changed from quint16 to qint16. check if valid
        if (!readInt16(b)) {
            qDebug() << "failed to read 16bit size";
            return false;
        }
        size = b;
    } else {
        qDebug() << "Invalid list size in readListSize: token" << QString::number(token);
        harakiri();
        return false;
    }
    return true;
}

bool BinTreeNodeReader::fillRawBuffer(quint32 stanzaSize)
{
    return fillArrayFromRawStream(rawBuffer, stanzaSize);
}

bool BinTreeNodeReader::fillArray(QByteArray& buffer, quint32 len)
{
    buffer = decodedStream.read(len);
    return buffer.size() == len;
}

bool BinTreeNodeReader::fillArrayFromRawStream(QByteArray& buffer, quint32 len)
{
    char data[1025];

    buffer.clear();
    buffer.reserve(len);

    bool ready = true;

    if (socket->bytesAvailable() < 1)
    {
        ready = socket->waitForReadyRead(-1);
    }

    if (!ready)
    {
        qDebug() << "fillArray() not ready / timed out";
        harakiri();
        return false;
    }

    int needToRead = len;
    while (needToRead > 0)
    {
        int bytesRead = socket->read(data,(needToRead > 1024) ? 1024 : needToRead);

        if (bytesRead < 0) {
            qDebug() << "bytesRead < 0" << socket->errorString();
            harakiri();
            return false;
        }
        if (bytesRead == 0) {
            socket->waitForReadyRead(-1);
        }
        else
        {
            needToRead -= bytesRead;
            buffer.append(data, bytesRead);
        }
    }
    return true;
}

bool BinTreeNodeReader::readString(QByteArray& s)
{
    quint8 token;
    if (!readInt8(token)) {
        qDebug() << "failed to read string token";
        return false;
    }
    return readString(token, s);
}

bool BinTreeNodeReader::readString(int token, QByteArray& s)
{
    if (token == -1) {
        qDebug() << "-1 token in readString";
        harakiri();
    }

    if (token > 2 && token < dict->dictSize(0))
        return getToken(token, s);

    //no default value.
    switch (token)
    {
        case 0:
            return false;

        case 252: {
            quint8 size8;
            if (!readInt8(size8))
                return false;
            return fillArray(s, size8);
        }
        case 253: {
            qint32 size24;
            if (!readInt24(size24))
                return false;
            return fillArray(s, size24);
        }
        case 254: {
            quint8 token8;
            if (!readInt8(token8))
                return false;
            return getToken(dict->dictSize(0) + token8, s);
        }
        case 250: {
            QByteArray user,server;
            bool usr = readString(user);
            bool srv = readString(server);
            if (usr && srv)
            {
                s = user + "@" + server;
                return true;
            }
            if (srv)
            {
                s = server;
                return true;
            }
            qDebug() << "readString couldn't reconstruct jid.";
            harakiri();
            return false;
        }
        case 251:
        case 255: {
            quint8 nbyte;
            if (!readInt8(nbyte))
                return false;
            int size = nbyte & 0x7f;
            int numnibbles = size * 2 - ((nbyte & 0x80) ? 1 : 0);

            QByteArray res;
            if (!fillArray(res, size))
                return false;
            res = res.toHex().left(numnibbles);
            res = res.replace('a', '-').replace('b', '.');
            s = res;
            return true;
        }
    }
    qDebug() << "readString invalid token" << QString::number(token);
    harakiri();
    return false;
}

bool BinTreeNodeReader::getToken(int token, QByteArray &s)
{
    int subdict = 0;
    QString string;
    dict->getToken(string, subdict, token);
    if (string.isEmpty()) {
        quint8 ext;
        if (!readInt8(ext))
            return false;
        dict->getToken(string, subdict, ext);
        if (!string.isEmpty()) {
            s = string.toUtf8();
            return true;
        }
    }
    else {
        s = string.toUtf8();
        return true;
    }

    qDebug() << "Invalid token/length in getToken.";
    harakiri();
    return false;
}

bool BinTreeNodeReader::readInt8(quint8 &byte)
{
    if (decodedStream.read(reinterpret_cast<char*>(&byte), 1) != 1) {
        return false;
    }
    return true;
}


qint32 BinTreeNodeReader::readRawInt16()
{
    char buffer[2];

    QByteArray readBytes(buffer, 2);

    fillArrayFromRawStream(readBytes, 2);

    qint32 result = (((quint8)readBytes.at(0)) << 8) + (quint8)readBytes.at(1);

    return (result);
}

qint32 BinTreeNodeReader::readRawInt24()
{
    char buffer[3];

    QByteArray readBytes(buffer, 3);

    fillArrayFromRawStream(readBytes, 3);

    qint32 result = ((((quint8)readBytes.at(0)) << 16) + ((quint8)readBytes.at(1) << 8) +
                     ((quint8)readBytes.at(2)));

    return (result);
}


bool BinTreeNodeReader::readInt16(qint16 &val)
{
    quint8 r1;
    quint8 r2;

    if (!readInt8(r1)
            || !readInt8(r2)) {
        return false;
    }

    val = (r1 << 8) + r2;
    return true;
}

bool BinTreeNodeReader::readInt24(qint32 &val)
{
    quint8 r1;
    quint8 r2;
    quint8 r3;

    if (!readInt8(r1)
            || !readInt8(r2)
            || !readInt8(r3)) {
        return false;
    }

    val = (r1 << 16) + (r2 << 8) + r3;
    return true;
}

void BinTreeNodeReader::setInputKey(KeyStream *inputKey)
{
    this->inputKey = inputKey;
}

void BinTreeNodeReader::harakiri()
{
    QObject::disconnect(socket, 0, 0, 0);
    socket->disconnectFromHost();
    Q_EMIT socketBroken();
}


