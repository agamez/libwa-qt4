#ifndef FUNREQUEST_H
#define FUNREQUEST_H

#include <QObject>

#include "protocoltreenode.h"

class FunRequest : public QObject
{
    Q_OBJECT
public:
    explicit FunRequest(QObject *parent = 0);

signals:
    void reply(const ProtocolTreeNode &m_node);
};

#endif // FUNREQUEST_H
