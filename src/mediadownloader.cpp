#include "mediadownloader.h"

#include <QDebug>

MediaDownloader::MediaDownloader(const QString &userAgent, QObject *parent) :
    QObject(parent),
    _userAgent(userAgent)
{
    _file = NULL;
    _nam = NULL;
}

MediaDownloader::~MediaDownloader()
{
}

void MediaDownloader::setData(const QString &jid, const QString &from, const QString &to, const QString &msgid)
{
    _jid = jid;
    _source = from;
    _destination = to;
    _msgid = msgid;
    _read = 0;
    _total = 0;
}

void MediaDownloader::download()
{
    qDebug() << _source << _destination << _msgid << _userAgent;
    _file = new QFile(_destination, this);
    if (_file->open(QFile::WriteOnly)) {
        _file->resize(0);
        _file->seek(0);
        _nam = new QNetworkAccessManager(this);
        QUrl url(_source);
        QNetworkRequest req(url);
        req.setRawHeader("User-Agent", _userAgent.toLatin1());
        req.setRawHeader("Connection", "closed");
        QNetworkReply *reply = _nam->get(req);
        QObject::connect(reply, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(onDownloadProgress(qint64,qint64)));
        QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
        QObject::connect(reply, SIGNAL(finished()), this, SLOT(onFinished()));
        QObject::connect(reply, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
        QObject::connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(onSslErrors(QList<QSslError>)));
    }
    else {
        qWarning() << "Can't open file:" << _destination;
    }
}

void MediaDownloader::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    //qDebug() << bytesReceived << bytesTotal;
    if (_total == 0) {
        _total = bytesTotal;
    }
    Q_EMIT progress(_jid, (float)bytesReceived / bytesTotal, _msgid);
}

void MediaDownloader::onError(QNetworkReply::NetworkError error)
{
    //qDebug() << error;

    _file->flush();
    _file->close();
    _file->remove();

    Q_EMIT failed(_jid, _msgid, this);
}

void MediaDownloader::onFinished()
{
    //qDebug() << "";

    _file->flush();
    _file->close();

    Q_EMIT completed(_jid, _destination, _msgid, this);
}

void MediaDownloader::onReadyRead()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply) {
        qint64 bytes = reply->bytesAvailable();
        _read += bytes;
        //qDebug() << bytes << _read << _total;
        _file->write(reply->readAll());
        if (_total > 0) {
            //Q_EMIT progress(_jid, (float)_read / _total, _msgid);
        }
    }
}

void MediaDownloader::onSslErrors(const QList<QSslError> &errors)
{
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    qDebug() << errors;
#endif

    _file->flush();
    _file->close();
    _file->remove();

    Q_EMIT failed(_jid, _msgid, this);
}
