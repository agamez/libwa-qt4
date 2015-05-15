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

#include <QTextStream>

#include "attributelistiterator.h"
#include "protocoltreenode.h"
#include "protocoltreenodelistiterator.h"

ProtocolTreeNode::ProtocolTreeNode(QObject *parent) :
    QObject(parent)
{
}

ProtocolTreeNode::ProtocolTreeNode(ProtocolTreeNode *node, QObject *parent) :
    QObject(parent)
{
    this->tag = node->tag;
    this->data = node->data;
    this->attributes = node->attributes;
    this->children = node->children;
    this->size = node->size;
}

ProtocolTreeNode::ProtocolTreeNode(const QString &tag, const AttributeList &attrs, const QByteArray &data, QObject *parent) :
    QObject(parent)
{
    this->tag = tag;
    this->attributes = attrs;
    this->data = data;
}

ProtocolTreeNode::ProtocolTreeNode(const QString &tag, const AttributeList &attrs, QObject *parent) :
    QObject(parent)
{
    this->tag = tag;
    this->attributes = attrs;
}

ProtocolTreeNode::ProtocolTreeNode(const ProtocolTreeNode &node, QObject *parent) :
    QObject(parent)
{
    this->tag = node.tag;
    this->data = node.data;
    this->attributes = node.attributes;
    this->children = node.children;
    this->size = node.size;
}


ProtocolTreeNode::ProtocolTreeNode(const QString &tag, QObject *parent) :
    QObject(parent)
{
    this->tag = tag;
}

ProtocolTreeNode::ProtocolTreeNode(const QString &tag, const QByteArray &data, QObject *parent) :
    QObject(parent)

{
    this->tag = tag;
    this->data = data;
}

void ProtocolTreeNode::addChild(const ProtocolTreeNode& child)
{
    children.insert(child.getTag(),child);
}

void ProtocolTreeNode::setTag(const QString &tag)
{
    this->tag = tag;
}

void ProtocolTreeNode::setData(const QByteArray &data)
{
    this->data = data;
}

void ProtocolTreeNode::setAttributes(const AttributeList &attribs)
{
    attributes.clear();

    AttributeListIterator i(attribs);
    while (i.hasNext())
    {
        i.next();
        attributes.insert(i.key(), i.value());
    }
}

int ProtocolTreeNode::getAttributesCount() const
{
    return attributes.size();
}

int ProtocolTreeNode::getChildrenCount() const
{
    return children.size();
}

const AttributeList& ProtocolTreeNode::getAttributes() const
{
    return attributes;
}

QString ProtocolTreeNode::getAttributeValue(const QString &key) const
{
    return attributes.value(key).toString();
}

const ProtocolTreeNodeList &ProtocolTreeNode::getChildren() const
{
    return children;
}

ProtocolTreeNode ProtocolTreeNode::getChild(const QString &tag) const
{
    return children.value(tag);
}

const QByteArray& ProtocolTreeNode::getData() const
{
    return data;
}

QString ProtocolTreeNode::getDataString() const
{
    return QString::fromUtf8(data);
}


const QString& ProtocolTreeNode::getTag() const
{
    return tag;
}

QString ProtocolTreeNode::toString(int depth) const
{
    QString result;
    QTextStream out(&result);

    out << "\n";
    out << QString("").leftJustified(depth * 4, ' ', false);
    out << "<" << tag << attributes.toString();

    if (children.size() > 0)
    {
        out << ">";
        if (data.length() > 0) {
            out << "\n";
            out << QString("").leftJustified((depth + 1) * 4, ' ', false);
            out << "data:hex:" << data.toHex();
        }
        ProtocolTreeNodeListIterator i(children);
        while (i.hasNext())
        {
            ProtocolTreeNode node = i.next().value();
            out << node.toString(depth + 1);
        }
        out << "\n";
        out << QString("").leftJustified(depth * 4, ' ', false);
        out << "</" << tag << ">";
    }
    else {
        if (data.length() > 0) {
            out << ">";
            out << "\n";
            out << QString("").leftJustified((depth + 1) * 4, ' ', false);
            out << "data:hex:" << data.toHex();
            out << "\n";
            out << QString("").leftJustified((depth) * 4, ' ', false);
            out << "</" << tag << ">";
        }
        else {
            out << " />";
        }
    }

    return result;
}

void ProtocolTreeNode::setSize(int size)
{
    this->size = size;
}

int ProtocolTreeNode::getSize() const
{
    return size;
}


