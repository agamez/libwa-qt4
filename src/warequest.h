#ifndef WAREQUEST_H
#define WAREQUEST_H

#include <QObject>

#include <QNetworkReply>
#include <QUrlQuery>
#include <QNetworkAccessManager>

class WARequest : public QObject
{
    Q_OBJECT
public:
    explicit WARequest(const QString &url, const QString &ua, QObject *parent = 0);

    void addParam(QString name, QString value);
    void sendRequest();

signals:
    void finished(const QVariantMap &reply);
    void httpError(const QString &errorString);
    void sslError();

public slots:
    void onResponse();
    void readResult();
    void error(QNetworkReply::NetworkError code);
    void sslErrors(const QList<QSslError> &);

private:
    QUrl m_url;
    QUrlQuery m_url_query;
    QString m_ua;
    QNetworkReply *m_reply;
    QNetworkAccessManager *m_nam;

};

#endif // WAREQUEST_H
