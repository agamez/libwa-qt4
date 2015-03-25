#ifndef WAREQUEST_H
#define WAREQUEST_H

#include <QObject>

#include <QNetworkReply>
#include <QNetworkAccessManager>

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#include <QUrlQuery>
#endif

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
    QString m_ua;
    QNetworkReply *m_reply;
    QNetworkAccessManager *m_nam;

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    QUrlQuery m_url_query;
#endif

};

#endif // WAREQUEST_H
