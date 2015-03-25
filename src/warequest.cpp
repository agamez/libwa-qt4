#include "warequest.h"

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#include <QJsonDocument>
#include <QJsonParseError>
#else
#include "qjson/parser.h"
#endif

#include <QDebug>

WARequest::WARequest(const QString &url, const QString &ua, QObject *parent) :
    QObject(parent)
{
    m_url = QUrl(url);
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    m_url_query.clear();
#endif
    m_ua = ua;
    m_reply = 0;
    m_nam = new QNetworkAccessManager(this);
}

void WARequest::addParam(QString name, QString value)
{
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    m_url_query.addQueryItem(name, QUrl::toPercentEncoding(value));
#else
    m_url.addQueryItem(name,  QUrl::toPercentEncoding(value));
#endif
}

void WARequest::sendRequest()
{
    if (m_nam) {
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
        m_url.setQuery(m_url_query);
#endif
        QNetworkRequest request(m_url);
        request.setRawHeader("User-Agent", m_ua.toLatin1());
        request.setRawHeader("Connection", "closed");
        qDebug() << "User-Agent" << m_ua;
        qDebug() << "WARequest:" << request.url().toString();

        m_reply = m_nam->get(request);
        QObject::connect(m_reply, SIGNAL(finished()), this, SLOT(onResponse()));
        QObject::connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(error(QNetworkReply::NetworkError)));
        QObject::connect(m_reply, SIGNAL(sslErrors(const QList<QSslError>)), this, SLOT(sslErrors(const QList<QSslError>)));
    }
}

void WARequest::onResponse()
{
    if (m_reply->error() == QNetworkReply::NoError) {
        if (m_reply->bytesAvailable())
            readResult();
        else {
            if (m_reply) {
                QObject::connect(m_reply,SIGNAL(readyRead()),this,SLOT(readResult()));
            }
            else {
                qDebug() << "broken reply";
                Q_EMIT httpError("Unknown error");
                m_reply->deleteLater();
            }
        }
    }
    else {
        Q_EMIT httpError(m_reply->errorString());
        m_reply->deleteLater();
    }
}

void WARequest::readResult()
{
    QByteArray json = m_reply->readAll();
    qDebug() << "Reply:" << json;

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error == QJsonParseError::NoError) {
        QVariantMap mapResult = doc.toVariant().toMap();
        Q_EMIT finished(mapResult);
    }
    else {
        Q_EMIT httpError("Cannot parse json reply");
    }
#else
    bool no_error = true;
    QJson::Parser parser;
    QVariant doc = parser.parse(json,&no_error);
    if (no_error) {
        QVariantMap mapResult = doc.toMap();
        Q_EMIT finished(mapResult);
    }
    else {
        Q_EMIT httpError("Cannot parse json reply");
    }
#endif
    m_reply->deleteLater();
}

void WARequest::error(QNetworkReply::NetworkError code)
{
    qDebug() << "WARequest network error:" << m_reply->errorString() << QString::number(code);
    Q_EMIT httpError(m_reply->errorString());
    m_reply->deleteLater();
}

void WARequest::sslErrors(const QList<QSslError> &)
{
    qDebug() << "WARequest ssl error";
    Q_EMIT sslError();
    m_reply->deleteLater();
}
