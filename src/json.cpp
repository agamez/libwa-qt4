#include "json.h"
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#include <QJsonDocument>
#include <QJsonParseError>
#else
#include "qtjson.h"
#endif

QByteArray Json::serialize(const QVariant &json)
{
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    QJsonDocument doc = QJsonDocument::fromVariant(json);
    return doc.toJson();
#else
    return QtJson::serialize(json);
#endif
}

QVariant Json::parse(const QByteArray &serialized)
{
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(serialized, &error);
    if (error.error == QJsonParseError::NoError) {
        return doc.toVariant();
    }
    else {
        return QVariant();
    }
#else
    bool no_error = true;
    QVariant doc = QtJson::parse(json, no_error);
    if (no_error) {
        return doc;
    }
    else {
        return QVariant();
    }
#endif
}
