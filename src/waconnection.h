#ifndef WACONNECTION_H
#define WACONNECTION_H

#include <QObject>
#include <QVariantMap>
#include <QSqlDatabase>

#include "libwa.h"
#include "protocoltreenode.h"

#define WAConnectionStatic (WAConnection::instance())

class WAConnectionPrivate;
class LIBWA_API WAConnection : public QObject
{
    Q_OBJECT
public:
    explicit WAConnection(QObject *parent = 0);
    virtual ~WAConnection();
    static WAConnection *GetInstance(QObject *parent = 0);

    enum ConnectionStatus {
        Disconnected,
        Connecting,
        Connected,
        Initiaization,
        LoggedIn
    };

public slots:
    void init();

    int getConnectionStatus();

    void login(const QVariantMap &loginData);
    void logout();
    void reconnect();
    void sendGetProperties();
    void sendPing();

    void sendText(const QString &jid, const QString &text);
    void sendBroadcastText(const QString &jid, const QString &text, const QStringList &jids);
    void sendMessageRead(const QString &jid, const QString &msgId, const QString &participant);
    void syncContacts(const QVariantMap &contacts);
    void sendGetStatuses(const QStringList &jids);
    void sendGetPictureIds(const QStringList &jids);
    void sendGetPicture(const QString &jid);
    void sendGetLastSeen(const QString &jid);
    void sendSubscribe(const QString &jid);
    void sendUnsubscribe(const QString &jid);
    void sendAvailable(const QString &pushname);
    void sendUnavailable();
    void sendGetGroups(const QString &type);
    void sendRetryMessage(const QString &jid, const QString &msgId, const QString &data);
    void sendTyping(const QString &jid, bool typing);

    void getEncryptionStatus(const QString &jid);

protected:
    WAConnectionPrivate * d_ptr;

private:
    Q_DECLARE_PRIVATE(WAConnection)
    int sendRequest(const ProtocolTreeNode &node, const char *member);
    int sendRequest(const ProtocolTreeNode &node);

    int m_connectionStatus;

private slots:

signals:
    void authSuccess(const AttributeList &accountData);
    void accountExpired(const AttributeList &accountData);
    void authFailed();
    void streamError();
    void notifyPushname(const QString &jid, const QString &pushname);
    void serverProperties(const QVariantMap &props);
    void notifyOfflineMessages(int count);
    void contactStatus(const QString &jid, const QString &status, const QString &t);
    void contactsStatuses(const QVariantMap &contacts);
    void contactLastSeenHidden(const QString &jid, const QString &code);
    void contactLastSeen(const QString &jid, uint seconds);
    void contactPictureHidden(const QString &jid, const QString &code);
    void contactPicture(const QString &jid, const QByteArray &pictureData, const QString &id);
    void contactPictureId(const QString &jid, const QString &id, const QString &author);
    void contactsPictureIds(const QVariantMap &contacts);
    void broadcastsReceived(const QVariantMap &broadcasts);
    void groupParticipantAdded(const QString &gjid, const QString &jid);
    void groupParticipantRemoved(const QString &gjid, const QString &jid);
    void groupParticipantPromoted(const QString &gjid, const QString &jid);
    void groupParticipantDemoted(const QString &gjid, const QString &jid);
    void groupSubjectChanged(const QString &gjid, const QString &subject, const QString &s_o, const QString &s_t);
    void groupCreated(const QString &gjid, const QString &creation, const QString &creator, const QString &s_o, const QString &s_t, const QString &subject, const QStringList &participants, const QStringList &admins);
    void messageSent(const QString &jid, const QString &msgId, const QString &timestamp);
    void retryMessage(const QString &msgId, const QString &jid);
    void messageReceipt(const QString &jid, const QString &msgId, const QString &participant, const QString &timestamp, const QString &type);
    void contactsSynced(const QVariantMap &jids);
    void contactAvailable(const QString &jid);
    void contactUnavailable(const QString &jid, const QString &last);
    void groupsReceived(const QVariantMap &groups);
    void blacklistReceived(const QStringList &list);

    void incomingCall(const QString &jid, const QString &timestamp);

    void contactTypingPaused(const QString &jid);
    void contactTypingStarted(const QString &jid);

    void textMessageReceived(const QString &jid, const QString &id, const QString &timestamp, const QString &author, bool offline, const QString &data);
    void mediaMessageReceived(const QString &jid, const QString &id, const QString &timestamp, const QString &author, bool offline, const AttributeList &attrs, const QByteArray &data);
    void textMessageSent(const QString &jid, const QString &id, const QString &timestamp, const QString &data);

    void connectionStatusChanged(int newConnectionStatus);
    void contactsNotification(const QString &type, const AttributeList &attributes);

    void encryptionStatus(const QString &jid, bool encrypted);

};

#endif // WACONNECTION_H
