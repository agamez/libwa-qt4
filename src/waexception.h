#ifndef WAEXCEPTION_H
#define WAEXCEPTION_H

#include <QException>
#include <QString>

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
