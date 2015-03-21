#ifndef WAREGISTRATION_H
#define WAREGISTRATION_H

#include <QObject>

#include "libwa.h"

class LIBWA_API WARegistration : public QObject
{
    Q_OBJECT
public:
    explicit WARegistration(QObject *parent = 0);
    void init(const QString &cc, const QString &phone,
              const QString &mcc, const QString &mnc,
              const QString &id, const QString &ua,
              const QString &method = QString(), const QString &token = QString());

    void checkExists();
    void codeRequest();
    void enterCode(const QString &code);

signals:
    void finished(const QVariantMap &reply);

private slots:
    void errorHandler(const QString &errorString);
    void sslError();

private:
    QString m_cc;
    QString m_phone;
    QString m_mcc;
    QString m_mnc;
    QString m_id;
    QString m_ua;
    QString m_method;
    QString m_token;
    QString m_lg;
    QString m_lc;

};

#endif // WAREGISTRATION_H
