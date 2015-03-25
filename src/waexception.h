#ifndef WAEXCEPTION_H
#define WAEXCEPTION_H

#include <QString>

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
#include <QException>
#else
#include "qexception/qexception.h"
#endif

class WAException : public QException
{
public:
    WAException(const QString &error = QString("Unknown error")) throw() {
        _error = error;
    }
    QString errorMessage() const {
        return _error;
    }
    WAException(const WAException &source) {
        _error = source.errorMessage();
    }
    virtual ~WAException() throw() {}

    void raise() const { throw *this; }
    WAException *clone() const { return new WAException(*this); }

protected:
    QString _error;
};

#endif // WAEXCEPTION_H
