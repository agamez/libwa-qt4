#include "warequest.h"

#include <QJsonDocument>
#include <QJsonParseError>

#include <QDebug>

WARequest::WARequest(const QString &url, const QString &ua, QObject *parent) :
    QObject(parent)
{
    m_url = QUrl(url);
    m_url_query.clear();
    m_ua = ua;
    m_reply = 0;
    m_nam = new QNetworkAccessManager(this);
}

void WARequest::addParam(QString name, QString value)
{
    m_url_query.addQueryItem(name, QUrl::toPercentEncoding(value));
}

void WARequest::sendRequest()
{
    if (m_nam) {
        m_url.setQuery(m_url_query);
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

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error == QJsonParseError::NoError) {
        QVariantMap mapResult = doc.toVariant().toMap();
        Q_EMIT finished(mapResult);
    }
    else {
        Q_EMIT httpError("Cannot parse json reply");
    }
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
