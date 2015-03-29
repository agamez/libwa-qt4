#ifndef JSON_H
#define JSON_H

#include <QString>
#include <QVariant>

class Json
{
public:
    static QByteArray serialize(const QVariant &json);
    static QVariant parse(const QByteArray &serialized);
};

#endif // JSON_H
