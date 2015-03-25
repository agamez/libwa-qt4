#include "waregistration.h"
#include "warequest.h"
#include "waconstants.h"

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#include <QJsonDocument>
#include <QJsonParseError>
#include <QUrlQuery>
#endif

#include <QNetworkAccessManager>
#include <QStringList>

#include <QLocale>

WARegistration::WARegistration(QObject *parent) :
    QObject(parent)
{
    QString locale = QLocale::system().name();
    m_lg = locale.split("_").first().toLower();
    m_lc = locale.split("_").length() > 1 ? locale.split("_").last().toUpper() : "ZZ";
}

void WARegistration::init(const QString &cc, const QString &phone, const QString &mcc, const QString &mnc, const QString &id, const QString &ua, const QString &method, const QString &token)
{
    m_cc = cc;
    m_phone = phone;
    m_mcc = mcc.rightJustified(3, '0');
    m_mnc = mnc.rightJustified(3, '0');
    m_id = id;
    m_ua = ua;
    m_method = method;
    m_token = token;
}

void WARegistration::checkExists()
{
    QString url = QString("%1%2").arg(REGISTRATION_URL).arg(REGISTRATION_EXIST);
    WARequest *request = new WARequest(url, m_ua, this);

    request->addParam("cc", m_cc);
    request->addParam("in", m_phone);
    request->addParam("lg", m_lg.isEmpty() || m_lg == "c" ? "en" : m_lg);
    request->addParam("lc", m_lc.isEmpty() ? "ZZ" : m_lc);
    request->addParam("token", m_token);
    request->addParam("id", m_id);

    QObject::connect(request, SIGNAL(finished(QVariantMap)), this, SIGNAL(finished(QVariantMap)));
    QObject::connect(request,SIGNAL(sslError()), this, SLOT(sslError()));
    QObject::connect(request,SIGNAL(httpError(QString)), this, SLOT(errorHandler(QString)));
    request->sendRequest();
}

void WARegistration::codeRequest()
{
    QString url = QString("%1%2").arg(REGISTRATION_URL).arg(REGISTRATION_CODE);
    WARequest *request = new WARequest(url, m_ua, this);

    request->addParam("cc", m_cc);
    request->addParam("in", m_phone);
    request->addParam("method", m_method);
    request->addParam("sim_mcc", m_mcc);
    request->addParam("sim_mnc", m_mnc);
    request->addParam("lg", m_lg.isEmpty() || m_lg == "c" ? "en" : m_lg);
    request->addParam("lc", m_lc.isEmpty() ? "ZZ" : m_lc);
    request->addParam("token", m_token);
    request->addParam("id", m_id);

    QObject::connect(request, SIGNAL(finished(QVariantMap)), this, SIGNAL(finished(QVariantMap)));
    QObject::connect(request,SIGNAL(sslError()), this, SLOT(sslError()));
    QObject::connect(request,SIGNAL(httpError(QString)), this, SLOT(errorHandler(QString)));
    request->sendRequest();
}

void WARegistration::enterCode(const QString &code)
{
    QString url = QString("%1%2").arg(REGISTRATION_URL).arg(REGISTRATION_VERIFY);
    WARequest *request = new WARequest(url, m_ua, this);

    request->addParam("cc", m_cc);
    request->addParam("in", m_phone);
    request->addParam("id", m_id);
    request->addParam("code", code);

    QObject::connect(request, SIGNAL(finished(QVariantMap)), this, SIGNAL(finished(QVariantMap)));
    QObject::connect(request,SIGNAL(sslError()), this, SLOT(sslError()));
    QObject::connect(request,SIGNAL(httpError(QString)), this, SLOT(errorHandler(QString)));
    request->sendRequest();
}

void WARegistration::errorHandler(const QString &errorString)
{
    QVariantMap result;

    result.insert("reason", errorString);
    Q_EMIT finished(result);
}

void WARegistration::sslError()
{
    QVariantMap result;

    result.insert("reason", "ssl_error");
    Q_EMIT finished(result);
}

