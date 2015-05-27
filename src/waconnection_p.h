#ifndef WACONNECTION_P_H
#define WACONNECTION_P_H

#include <QObject>
#include <QHash>
#include <QTcpSocket>
#include <QMutex>
#include <QReadWriteLock>

#include "waconnection.h"

#include "protocoltreenode.h"
#include "bintreenodewriter.h"
#include "bintreenodereader.h"
#include "keystream.h"
#include "watokendictionary.h"

#include "axolotl/liteaxolotlstore.h"

#include "../libaxolotl/sessioncipher.h"

#define WAREPLY(a) #a

#define AXOLOTL_DB_CONNECTION "qt_sql_axolotl_connection"

class WAConnectionPrivate : public QObject
{
    Q_OBJECT
public:
    explicit WAConnectionPrivate(WAConnection *q);

    void login(const QVariantMap &loginData);
    void logout();
    void reconnect();
    void sendGetProperties();
    void sendPing();
    void sendGetLastSeen(const QString &jid);
    void sendGetStatuses(const QStringList &jids);
    void sendGetPicture(const QString &jid);
    void sendGetPictureIds(const QStringList &jids);
    void sendPresenceRequest(const QString &jid, const QString &type);
    void sendGetPrivacyList();
    void sendSetPrivacyList(const QStringList &blockedJids, const QStringList &spamJids = QStringList());
    void sendGetPushConfig();

    void sendSetPresence(bool available, const QString &pushname = QString());

    void sendText(const QString &jid, const QString &text, const QString &msgId = QString());
    void sendBroadcastText(const QString &jid, const QString &text, const QStringList &jids);
    void sendMessageRead(const QString &jid, const QString &msgId, const QString &participant);
    void sendRetryMessage(const QString &jid, const QString &msgId, const QString &data);

    void syncContacts(const QVariantMap &contacts);

    void sendSync(const QStringList &syncContacts, const QStringList &deleteJids, int syncType = 4, int index = 0, bool last = true);
    void sendGetFeatures(const QStringList &jids);
    void sendGetGroups(const QString &type);
    void sendGetBroadcasts();

    void sendSetStatusMessage(const QString &message);

    void sendTyping(const QString &jid, bool typing);

    void getEncryptionStatus(const QString &jid);

    int sendRequest(const ProtocolTreeNode &node);
    int sendRequest(const ProtocolTreeNode &node, const char *member);

public slots:
    void connectionServerProperties(const ProtocolTreeNode &node);
    void contactLastSeen(const ProtocolTreeNode &node);
    void contactsStatuses(const ProtocolTreeNode &node);
    void contactPicture(const ProtocolTreeNode &node);
    void contactsPicrureIds(const ProtocolTreeNode &node);
    void syncResponse(const ProtocolTreeNode &node);
    void groupsResponse(const ProtocolTreeNode &node);
    void broadcastsResponse(const ProtocolTreeNode &node);
    void passiveResponse(const ProtocolTreeNode &node);
    void encryptionReply(const ProtocolTreeNode &node);
    void onPong(const ProtocolTreeNode &node);
    void getKeysReponse(const ProtocolTreeNode &node);
    void messageSent(const ProtocolTreeNode &node);
    void featureResponse(const ProtocolTreeNode &node);
    void privacyList(const ProtocolTreeNode &node);
    void pushConfig(const ProtocolTreeNode &node);

private:
    Q_DECLARE_PUBLIC(WAConnection)
    WAConnection * const q_ptr;
    QSharedPointer<LiteAxolotlStore> axolotlStore;

    void tryLogin();
    int sendFeatures();
    int sendAuth();
    QByteArray getAuthBlob(const QByteArray &nonce);
    int sendResponse(const QByteArray &challengeData);
    void parseSuccessNode(const ProtocolTreeNode &node);
    void parseContactsNotification(const ProtocolTreeNode &node);
    void parsePictureNotification(const ProtocolTreeNode &node);
    void parseStatusNotification(const ProtocolTreeNode &node);
    void parseGroupNotification(const ProtocolTreeNode &node);
    void parseEncryptNotification(const ProtocolTreeNode &node);
    void parseReceipt(const ProtocolTreeNode &node);
    void parseReceiptList(const ProtocolTreeNode &node);
    void parsePresence(const ProtocolTreeNode &node);
    void parseChatstate(const ProtocolTreeNode &node);
    void parseCall(const ProtocolTreeNode &node);

    QString makeId();
    QString messageId();

    void sendClientConfig();
    void sendMessageReceived(const QString &jid, const QString &msdId, const QString &type = QString(), const QString &participant = QString());
    void sendMessageRetry(const QString &jid, const QString &msdId);
    void sendReceiptAck(const ProtocolTreeNode &node);
    void sendCleanDirty(const QStringList &categories);
    void sendNotificationReceived(const ProtocolTreeNode &node);
    void sendResult(const QString &id);
    void sendPassive(const QString &mode);
    void sendCallReceipt(const ProtocolTreeNode &node);
    void sendCallAck(const ProtocolTreeNode &node);
    void sendCallReject(const QString &jid, const QString &id, const QString &callId);

    bool parseMessage(const ProtocolTreeNode &node);
    bool parseEncryptedMessage(const ProtocolTreeNode &node);
    bool parsePreKeyWhisperMessage(const ProtocolTreeNode &node);
    bool parseWhisperMessage(const ProtocolTreeNode &node);

    SessionCipher *getSessionCipher(qulonglong recepient);

    ProtocolTreeNode getTextBody(const QString &text);
    ProtocolTreeNode getBroadcastNode(const QStringList &jids);
    ProtocolTreeNode getMessageNode(const QString &jid, const QString &type, const QString &msgId = QString());

    void sendEncrypt(bool fresh = true);
    void sendGetEncryptKeys(const QStringList &jids);

    qulonglong getRecepient(const QString &jid);

    bool read();

    bool m_isReading;
    bool m_authFailed;

    QReadWriteLock lockSeq;

    QHash<QString, const char*> m_bindStore;

    QTcpSocket *socket;
    QAbstractSocket::SocketError socketLastError;
    WATokenDictionary *dict;
    BinTreeNodeWriter *out;
    BinTreeNodeReader *in;
    KeyStream *outputKey;
    KeyStream *inputKey;
    uint iqid;
    uint mseq;
    uint sessionTime;
    int retry;

    int serverTimeCorrection;

    QHash<qulonglong, SessionCipher*> cipherHash;
    QHash<QString, QString> pendingMessages;
    QStringList skipEncodingJids;

    QStringList blacklist;

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
    QStringList m_servers;
    QString m_encryptionav;
    int m_passiveCount;
    bool m_passiveGroups;
    bool m_passiveReconnect;
    bool m_passive;
    QString m_jid;

private slots:
    void init();
    void loginInternal();

    void readNode();

    void socketConnected();
    void socketDisconnected();
    void socketError(QAbstractSocket::SocketError error);
};

#endif // WACONNECTION_P_H
