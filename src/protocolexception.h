#ifndef PROTOCOLEXCEPTION_H
#define PROTOCOLEXCEPTION_H

#include "waexception.h"

class ProtocolException : public WAException
{
public:
    ProtocolException(const QString &error): WAException(error) {}
};

#endif // PROTOCOLEXCEPTION_H
