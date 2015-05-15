#ifndef MEDIADOWNLOADER_H
#define MEDIADOWNLOADER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>

#include "libwa.h"

class LIBWA_API MediaDownloader : public QObject
{
    Q_OBJECT
public:
    explicit MediaDownloader(const QString &userAgent, QObject *parent = 0);
    virtual ~MediaDownloader();

    void setData(const QString &jid, const QString &from, const QString &to, const QString &msgid);

private:
    QString _userAgent;

    QString _jid;
    QString _source;
    QString _destination;
    QString _msgid;

    QFile *_file;

    QNetworkAccessManager *_nam;

    qint64 _read;
    qint64 _total;

public slots:
    void download();

private slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onError(QNetworkReply::NetworkError error);
    void onFinished();
    void onReadyRead();
    void onSslErrors(const QList<QSslError> &errors);

signals:
    void progress(const QString &jid, float value, const QString &msgid);
    void completed(const QString &jid, const QString &dest, const QString &msgid, MediaDownloader *object);
    void failed(const QString &jid, const QString &msgid, MediaDownloader *object);

};

#endif // MEDIADOWNLOADER_H
