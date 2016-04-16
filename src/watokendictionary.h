#ifndef WATOKENDICTIONARY_H
#define WATOKENDICTIONARY_H

#include <QStringList>

class WATokenDictionary : public QObject
{
    Q_OBJECT
public:
    WATokenDictionary(QObject * parent = 0);

    bool tryGetToken(const QString &string, int &subdict, int &token);
    void getToken(QString &string, int &subdict, int token);

    int dictSize(int) const;

private:
    QStringList dictStrings[3];
};

#endif // WATOKENDICTIONARY_H
