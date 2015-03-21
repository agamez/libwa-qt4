#ifndef WACONNECTION_P_H
#define WACONNECTION_P_H

#include <QObject>
#include <QHash>
#include <QTcpSocket>
#include <QMutex>
#include <QReadWriteLock>

#include "waconnection.h"

#include "funrequest.h"
#include "protocoltreenode.h"
#include "bintreenodewriter.h"
#include "bintreenodereader.h"
#include "keystream.h"
#include "watokendictionary.h"

#define WAREPLY(a) #a

class WAConnectionPrivate : public QObject
{
    Q_OBJECT
public:
    explicit WAConnectionPrivate(WAConnection *q);

    void login(const QVariantMap &loginData);
    void logout();
    void sendGetProperties();
    void sendPing();
    void sendGetLastSeen(const QString &jid);
    void sendGetStatuses(const QStringList &jids);
    void sendGetPicture(const QString &jid);
    void sendGetPictureIds(const QStringList &jids);

    void sendNormalText(const QString &jid, const QString &text);
    void sendBroadcastText(const QString &jid, const QString &text, const QStringList &jids);
    void sendMessageRead(const QString &jid, const QString &msgId, const QString &participant);
    void syncContacts(const QVariantMap &contacts);

    void sendSync(const QStringList &syncContacts, const QStringList &deleteJids, int syncType = 4, int index = 0, bool last = true);

    int sendRequest(const ProtocolTreeNode &node);
    int sendRequest(const ProtocolTreeNode &node, const char *member);

public slots:
    void connectionServerProperties(const ProtocolTreeNode &node);
    void contactLastSeen(const ProtocolTreeNode &node);
    void contactsStatuses(const ProtocolTreeNode &node);
    void contactPicture(const ProtocolTreeNode &node);
    void contactsPicrureIds(const ProtocolTreeNode &node);
    void syncResponse(const ProtocolTreeNode &node);

private:
    Q_DECLARE_PUBLIC(WAConnection)
    WAConnection * const q_ptr;

    void tryLogin();
    int sendFeatures();
    int sendAuth();
    QByteArray getAuthBlob(const QByteArray &nonce);
    int sendResponse(const QByteArray &challengeData);
    void parseSuccessNode(const ProtocolTreeNode &node);
    void parseContactsNotification(const ProtocolTreeNode &node);
    void parsePictureNotification(const ProtocolTreeNode &node);
    void parseSubjectNotification(const ProtocolTreeNode &node);
    void parseStatusNotification(const ProtocolTreeNode &node);
    void parseGroupNotification(const ProtocolTreeNode &node);

    QString makeId(const QString &prefix);
    QString messageId();

    void sendClientConfig();
    void sendMessageReceived(const QString &jid, const QString &msdId, const QString &type = QString(), const QString &participant = QString());
    void sendReceiptAck(const ProtocolTreeNode &node);
    void sendCleanDirty(const QStringList &categories);
    void sendNotificationReceived(const ProtocolTreeNode &node);
    void sendResult(const QString &id);

    void parseMessage(const ProtocolTreeNode &node);

    ProtocolTreeNode getTextBody(const QString &text);
    ProtocolTreeNode getBroadcastNode(const QStringList &jids);
    ProtocolTreeNode getMessageNode(const QString &jid, const QString &type);

    bool read();

    bool m_isReading;

    QReadWriteLock lockSeq;

    QHash<QString, const char*> m_bindStore;

    QTcpSocket *socket;
    WATokenDictionary *dict;
    BinTreeNodeWriter *out;
    BinTreeNodeReader *in;
    KeyStream *outputKey;
    KeyStream *inputKey;
    uint iqid;
    uint mseq;

    QVariantMap syncing;

    int connectionStatus;

    QString m_username;
    QByteArray m_password;
    QString m_resource;
    QString m_userAgent;
    QString m_mnc;
    QString m_mcc;
    QByteArray m_nextChallenge;
    QString m_domain;

private slots:
    void init();

    void readNode();

    void socketConnected();
    void socketDisconnected();
    void socketError(QAbstractSocket::SocketError error);
};

#endif // WACONNECTION_P_H
