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

#ifndef PROTOCOLTREENODE_H
#define PROTOCOLTREENODE_H

#include <QObject>
#include <QString>
#include <QMap>

#include "attributelist.h"
#include "protocoltreenodelist.h"

class ProtocolTreeNode : public QObject
{
    Q_OBJECT
public:
    explicit ProtocolTreeNode(QObject *parent = 0);
    ProtocolTreeNode(const ProtocolTreeNode &node, QObject *parent = 0);
    ProtocolTreeNode(ProtocolTreeNode *node, QObject *parent = 0);
    ProtocolTreeNode(const QString &tag, QObject *parent = 0);
    ProtocolTreeNode(const QString &tag, const QByteArray &data, QObject *parent = 0);
    ProtocolTreeNode(const QString &tag, const AttributeList &attrs, QObject *parent = 0);
    ProtocolTreeNode(const QString &tag, const AttributeList &attrs, const QByteArray &data, QObject *parent = 0);

    void addChild(const ProtocolTreeNode& child);
    void setTag(const QString &tag);
    void setData(const QByteArray &data);
    void setAttributes(const AttributeList &attribs);
    void setSize(int size);

    int getSize() const;
    int getAttributesCount() const;
    int getChildrenCount() const;
    const QByteArray& getData() const;
    QString getDataString() const;
    const QString& getTag() const;
    QString getAttributeValue(const QString &key) const;
    const AttributeList& getAttributes() const;
    const ProtocolTreeNodeList& getChildren() const;
    ProtocolTreeNode getChild(const QString &tag) const;
    QString toString(int depth = 0) const;

private:
    QString tag;
    QByteArray data;
    AttributeList attributes;
    ProtocolTreeNodeList children;
    int size;

};

#endif // PROTOCOLTREENODE_H
