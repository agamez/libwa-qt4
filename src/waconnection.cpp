#include "waconnection.h"
#include "waconnection_p.h"
#include "waconstants.h"
#include "protocoltreenodelistiterator.h"

#include "../libaxolotl/util/keyhelper.h"
#include "../libaxolotl/protocol/prekeywhispermessage.h"
#include "../libaxolotl/protocol/whispermessage.h"
#include "../libaxolotl/whisperexception.h"

#include <QDateTime>
#include <QMetaObject>
#include <QThread>
#include <QSqlError>
#include <QFile>
#include <QTimer>
#include <QUuid>

WAConnectionPrivate::WAConnectionPrivate(WAConnection *q):
    QObject(q),
    q_ptr(q),
    m_isReading(false),
    m_passive(false),
    m_passiveCount(0),
    m_passiveGroups(false),
    m_passiveReconnect(false),
    m_authFailed(false)
{
}

void WAConnectionPrivate::init()
{
    socket = new QTcpSocket(this);
    socketLastError = QAbstractSocket::UnknownSocketError;
    dict = new WATokenDictionary(this);
    out = new BinTreeNodeWriter(socket, dict, this);
    in = new BinTreeNodeReader(socket, dict, this);
    iqid = 0;
    mseq = 0;
    sessionTime = QDateTime::currentDateTime().toTime_t();

    axolotlStore.clear();
    axolotlStore = QSharedPointer<LiteAxolotlStore>(new LiteAxolotlStore(AXOLOTL_DB_CONNECTION));

    q_ptr->m_connectionStatus = WAConnection::Disconnected;
    Q_EMIT q_ptr->connectionStatusChanged(q_ptr->m_connectionStatus);
}

void WAConnectionPrivate::readNode()
{
    while (socket->state() == QAbstractSocket::ConnectedState && socket->bytesAvailable() > 0) {
        if (!read())
            qDebug() << "Error reading tree";
    }
}

void WAConnectionPrivate::login(const QVariantMap &loginData)
{
    if (q_ptr->m_connectionStatus > WAConnection::Disconnected) {
        return;
    }

    m_domain = QString(JID_DOMAIN);
    m_username = loginData["login"].toString();
    m_jid = QString("%1%2").arg(m_username).arg(m_domain);
    m_password = loginData["password"].toByteArray();
    m_password = QByteArray::fromBase64(m_password);
    m_resource = loginData["resource"].toString();
    m_encryptionav = loginData["encryptionav"].toString();
    m_userAgent = loginData["userAgent"].toString();
    m_mcc = loginData["mcc"].toString();
    m_mnc = loginData["mnc"].toString();
    m_nextChallenge = loginData["nextChallenge"].toByteArray();
    if (m_nextChallenge.size() > 0)
        m_nextChallenge = QByteArray::fromBase64(m_nextChallenge);
    QString database = loginData["database"].toString();
    axolotlStore->setDatabaseName(database);
    m_servers = loginData["servers"].toStringList();
    m_passive = loginData["passive"].toBool();
    if (m_passive) {
        qDebug() << "PASSIVE LOGIN!";
    }

    loginInternal();
}

void WAConnectionPrivate::logout()
{
    if (q_ptr->m_connectionStatus >= WAConnection::Initiaization) {
        sendSetPresence(false);
        out->streamEnd();
    }
    if (socket->isOpen()) {
        socket->disconnectFromHost();
    }
}

void WAConnectionPrivate::reconnect()
{
    if (!m_username.isEmpty()) {
        loginInternal();
    }
}

void WAConnectionPrivate::sendGetProperties()
{
    ProtocolTreeNode iqNode("iq");
    AttributeList attrs;
    attrs.insert("id", makeId());
    attrs.insert("type","get");
    attrs.insert("to", m_domain);
    attrs.insert("xmlns", "w");
    iqNode.setAttributes(attrs);

    ProtocolTreeNode propsNode("props");
    iqNode.addChild(propsNode);

    int bytes = sendRequest(iqNode, WAREPLY(connectionServerProperties));
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendPing()
{
    ProtocolTreeNode iqNode("iq");
    AttributeList attrs;
    attrs.insert("id", makeId());
    attrs.insert("to", m_domain);
    attrs.insert("type", "get");
    attrs.insert("xmlns", "w:p");
    iqNode.setAttributes(attrs);

    ProtocolTreeNode pingNode("ping");
    iqNode.addChild(pingNode);

    int bytes = sendRequest(iqNode, WAREPLY(onPong));
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendGetLastSeen(const QString &jid)
{
    if (jid.contains("-"))
        return;

    ProtocolTreeNode iqNode("iq");
    AttributeList attrs;
    attrs.insert("id", makeId());
    attrs.insert("type", "get");
    attrs.insert("to", jid);
    attrs.insert("xmlns", "jabber:iq:last");
    iqNode.setAttributes(attrs);

    ProtocolTreeNode queryNode("query");
    iqNode.addChild(queryNode);

    int bytes = sendRequest(iqNode, WAREPLY(contactLastSeen));
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendGetStatuses(const QStringList &jids)
{
    ProtocolTreeNode statusNode("status");

    AttributeList attrs;

    foreach (QString jid, jids) {
        if (jid.contains("@"))
        {
            ProtocolTreeNode userNode("user");
            attrs.clear();
            attrs.insert("jid", jid);
            userNode.setAttributes(attrs);
            statusNode.addChild(userNode);
        }
    }

    ProtocolTreeNode iqNode("iq");
    attrs.clear();
    attrs.insert("id", makeId());
    attrs.insert("to", m_domain);
    attrs.insert("type", "get");
    attrs.insert("xmlns", "status");
    iqNode.setAttributes(attrs);
    iqNode.addChild(statusNode);

    int bytes = sendRequest(iqNode, WAREPLY(contactsStatuses));
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendGetPicture(const QString &jid)
{
    ProtocolTreeNode iqNode("iq");
    AttributeList attrs;
    attrs.insert("id", makeId());
    attrs.insert("type", "get");
    attrs.insert("to", jid);
    attrs.insert("xmlns", "w:profile:picture");
    iqNode.setAttributes(attrs);

    ProtocolTreeNode pictureNode("picture");
    attrs.clear();
    attrs.insert("type", "image");
    pictureNode.setAttributes(attrs);
    iqNode.addChild(pictureNode);

    int bytes = sendRequest(iqNode, WAREPLY(contactPicture));
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendGetPictureIds(const QStringList &jids)
{
    ProtocolTreeNode iqNode("iq");
    AttributeList attrs;
    attrs.insert("id", makeId());
    attrs.insert("to", m_domain);
    attrs.insert("type", "get");
    attrs.insert("xmlns", "w:profile:picture");
    iqNode.setAttributes(attrs);

    ProtocolTreeNode listNode("list");
    foreach (QString jid, jids) {
        if (jid.contains("@"))
        {
            ProtocolTreeNode userNode("user");
            attrs.clear();
            attrs.insert("jid", jid);
            userNode.setAttributes(attrs);
            listNode.addChild(userNode);
        }
    }
    attrs.clear();
    iqNode.addChild(listNode);

    int bytes = sendRequest(iqNode, WAREPLY(contactsPicrureIds));
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendPresenceRequest(const QString &jid, const QString &type)
{
    qDebug() << jid << type;

    ProtocolTreeNode presenceNode("presence");

    AttributeList attrs;
    attrs.insert("to", jid);
    attrs.insert("type", type);
    presenceNode.setAttributes(attrs);

    int bytes = sendRequest(presenceNode);
}

void WAConnectionPrivate::sendGetPrivacyList()
{
    ProtocolTreeNode iqNode("iq");
    AttributeList attrs;
    attrs.insert("id", makeId());
    attrs.insert("type", "get");
    attrs.insert("xmlns", "jabber:iq:privacy");
    iqNode.setAttributes(attrs);

    ProtocolTreeNode listNode("list");
    attrs.clear();
    attrs.insert("name", "default");
    listNode.setAttributes(attrs);

    ProtocolTreeNode queryNode("query");
    queryNode.addChild(listNode);

    iqNode.addChild(queryNode);

    int bytes = sendRequest(iqNode, WAREPLY(privacyList));
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendSetPrivacyList(const QStringList &blockedJids, const QStringList &spamJids)
{
    ProtocolTreeNode listNode("list");
    AttributeList attrs;
    attrs.insert("name", "default");
    listNode.setAttributes(attrs);

    int order = 1;
    foreach(QString jid, blockedJids)
    {
        ProtocolTreeNode itemNode("item");
        attrs.clear();
        attrs.insert("type", "jid");
        attrs.insert("value", jid);
        attrs.insert("action", "deny");
        attrs.insert("order", QString::number(order++));
        itemNode.setAttributes(attrs);
        listNode.addChild(itemNode);
    }
    foreach(QString jid, spamJids)
    {
        ProtocolTreeNode itemNode("item");
        attrs.clear();
        attrs.insert("type", "jid");
        attrs.insert("value", jid);
        attrs.insert("action", "deny");
        attrs.insert("order", QString::number(order++));
        attrs.insert("reason", "spam");
        itemNode.setAttributes(attrs);
        listNode.addChild(itemNode);
    }

    ProtocolTreeNode queryNode("query");
    attrs.clear();
    queryNode.setAttributes(attrs);
    queryNode.addChild(listNode);

    ProtocolTreeNode iqNode("iq");
    attrs.clear();
    attrs.insert("id", makeId());
    attrs.insert("type", "set");
    attrs.insert("xmlns", "jabber:iq:privacy");
    iqNode.setAttributes(attrs);
    iqNode.addChild(queryNode);

    int bytes = sendRequest(iqNode);
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendGetPushConfig()
{
    ProtocolTreeNode iqNode("iq");
    AttributeList attrs;
    attrs.insert("id", makeId());
    attrs.insert("type", "get");
    attrs.insert("to", m_domain);
    attrs.insert("xmlns", "urn:xmpp:whatsapp:push");
    iqNode.setAttributes(attrs);

    ProtocolTreeNode configNode("config");
    iqNode.addChild(configNode);

    int bytes = sendRequest(iqNode, WAREPLY(pushConfig));
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendSetPresence(bool available, const QString &pushname)
{
    ProtocolTreeNode presenceNode("presence");

    AttributeList attrs;
    if (!pushname.isEmpty()) {
        attrs.insert("name", pushname);
    }
    attrs.insert("type", available ? "available" : "unavailable");
    presenceNode.setAttributes(attrs);

    int bytes = sendRequest(presenceNode);
}

void WAConnectionPrivate::sendText(const QString &jid, const QString &text, const QString &msgId)
{
    if (q_ptr->m_connectionStatus != WAConnection::LoggedIn) {
        return;
    }

    ProtocolTreeNode message = getMessageNode(jid, "text", msgId);
    if (m_resource.startsWith("S40") || !jid.contains(m_domain) || skipEncodingJids.contains(jid)) {
        ProtocolTreeNode body = getTextBody(text);
        message.addChild(body);
    }
    else {
        qulonglong recepientId = getRecepient(jid);
        if (axolotlStore->containsSession(recepientId, 1)) {
            SessionCipher *cipher = getSessionCipher(recepientId);
            qDebug() << "Ciphering" << text << "to" << recepientId << "with cihper" << cipher;
            QSharedPointer<CiphertextMessage> ciphertext = cipher->encrypt(text.toUtf8());

            ProtocolTreeNode encNode("enc");
            AttributeList attrs;
            attrs.insert("type", ciphertext->getType() == WHISPER_TYPE ? "msg" : "pkmsg");
            attrs.insert("av", m_encryptionav);
            attrs.insert("v", "1");
            encNode.setAttributes(attrs);
            encNode.setData(ciphertext->serialize());
            message.addChild(encNode);
        }
        else  {
            pendingMessages[jid] = text;
            sendGetEncryptKeys(QStringList() << jid);
            return;
        }
    }
    int bytes = sendRequest(message, WAREPLY(messageSent));

    Q_EMIT q_ptr->textMessageSent(jid, message.getAttributeValue("id"), QString::number(QDateTime::currentDateTime().toTime_t() + serverTimeCorrection), text);
}

void WAConnectionPrivate::sendBroadcastText(const QString &jid, const QString &text, const QStringList &jids)
{
    ProtocolTreeNode message = getMessageNode(jid, "text");

    ProtocolTreeNode broadcast = getBroadcastNode(jids);
    message.addChild(broadcast);

    ProtocolTreeNode body = getTextBody(text);
    message.addChild(body);

    int bytes = sendRequest(message);
}

void WAConnectionPrivate::sendMessageRead(const QString &jid, const QString &msgId, const QString &participant)
{
    sendMessageReceived(jid, msgId, QString("read"), participant);
}

void WAConnectionPrivate::sendRetryMessage(const QString &jid, const QString &msgId, const QString &data)
{
    sendText(jid, data, msgId);
}

void WAConnectionPrivate::syncContacts(const QVariantMap &contacts)
{
    syncing = contacts;
    sendSync(syncing.keys(), QStringList(), m_passive ? 0 : 3);
}

void WAConnectionPrivate::sendSync(const QStringList &syncContacts, const QStringList &deleteJids, int syncType, int index, bool last)
{
    QString mode("delta");
    QString context("background");

    switch (syncType) {
    case 0:
        mode = "full";
        context = "registration";
        break;
    case 1:
        mode = "full";
        context = "interactive";
        break;
    case 2:
        mode = "full";
        context = "background";
        break;
    case 3:
        mode = "delta";
        context = "interactive";
        break;
    case 4:
        mode = "delta";
        context = "background";
        break;
    case 5:
        mode = "query";
        context = "interactive";
        break;
    case 6:
        mode = "chunked";
        context = "registration";
        break;
    case 7:
        mode = "chunked";
        context = "interactive";
        break;
    case 8:
        mode = "chunked";
        context = "background";
        break;
    }

    ProtocolTreeNode syncNode("sync");
    AttributeList attrs;
    attrs.insert("mode", mode);
    attrs.insert("context", context);
    attrs.insert("index", QString::number(index));
    attrs.insert("last", last ? "true" : "false");
    QString uuid = QUuid::createUuid().toString().mid(1, 36);
    attrs.insert("sid", QString("sync_sid_%1_%2").arg(mode).arg(uuid));
    syncNode.setAttributes(attrs);

    foreach (QString syncNumber, syncContacts) {
        ProtocolTreeNode userNode("user");
        QString number = syncNumber.remove("p");
        userNode.setData(number.toLatin1());
        syncNode.addChild(userNode);
        if (!number.startsWith("+")) {
            ProtocolTreeNode extraNode("user");
            number = "+" + number;
            if (!syncing.contains("p" + number)) {
                syncing["p" + number] = syncing["p" + syncNumber];
            }
            extraNode.setData(number.toLatin1());
            syncNode.addChild(extraNode);
        }
    }

    foreach (const QString &deleteJid, deleteJids) {
        ProtocolTreeNode userNode("user");
        attrs.clear();
        attrs.insert("jid", deleteJid);
        attrs.insert("type", "delete");
        userNode.setAttributes(attrs);
        syncNode.addChild(userNode);
    }

    ProtocolTreeNode iqNode("iq");
    attrs.clear();
    attrs.insert("id", makeId());
    attrs.insert("xmlns", "urn:xmpp:whatsapp:sync");
    attrs.insert("type", "get");
    iqNode.setAttributes(attrs);
    iqNode.addChild(syncNode);

    int bytes = sendRequest(iqNode, WAREPLY(syncResponse));
}

void WAConnectionPrivate::sendGetFeatures(const QStringList &jids)
{
    ProtocolTreeNode iqNode("iq");
    AttributeList attrs;
    attrs.insert("id", makeId());
    attrs.insert("xmlns", "features");
    attrs.insert("type", "get");
    attrs.insert("to", m_domain);
    iqNode.setAttributes(attrs);

    ProtocolTreeNode featureNode("feature");

    ProtocolTreeNode listNode("list");
    foreach (const QString &jid, jids) {
        ProtocolTreeNode userNode("user");
        AttributeList userAttrs;
        userAttrs.insert("jid", jid);
        userNode.setAttributes(userAttrs);
        listNode.addChild(userNode);
    }
    featureNode.addChild(listNode);
    iqNode.addChild(featureNode);

    int bytes = sendRequest(iqNode, WAREPLY(featureResponse));
}

void WAConnectionPrivate::sendGetGroups(const QString &type)
{
    ProtocolTreeNode iqNode("iq");

    AttributeList attrs;
    attrs.insert("id", makeId());
    attrs.insert("type", "get");
    attrs.insert("to", "g.us");
    attrs.insert("xmlns", "w:g2");
    iqNode.setAttributes(attrs);

    ProtocolTreeNode childNode(type);
    iqNode.addChild(childNode);

    int bytes = sendRequest(iqNode, WAREPLY(groupsResponse));
}

void WAConnectionPrivate::sendGetBroadcasts()
{
    ProtocolTreeNode iqNode("iq");
    AttributeList attrs;
    attrs.insert("id", makeId());
    attrs.insert("xmlns", "w:b");
    attrs.insert("type", "get");
    attrs.insert("to", m_domain);
    iqNode.setAttributes(attrs);

    ProtocolTreeNode listsNode("lists");
    iqNode.addChild(listsNode);

    int bytes = sendRequest(iqNode, WAREPLY(broadcastsResponse));
}

void WAConnectionPrivate::sendSetStatusMessage(const QString &message)
{
    ProtocolTreeNode iqNode("iq");
    AttributeList attrs;
    attrs.insert("id", makeId());
    attrs.insert("xmlns", "status");
    attrs.insert("type", "set");
    attrs.insert("to", m_domain);
    iqNode.setAttributes(attrs);

    ProtocolTreeNode statusNode("status");
    statusNode.setData(message.toUtf8());

    iqNode.addChild(statusNode);

    int bytes = sendRequest(iqNode);
}

void WAConnectionPrivate::sendTyping(const QString &jid, bool typing)
{
    ProtocolTreeNode messageNode("chatstate");
    AttributeList attrs;
    attrs.insert("to", jid);
    messageNode.setAttributes(attrs);

    ProtocolTreeNode composingNode(typing ? "composing" : "paused");
    messageNode.addChild(composingNode);

    int bytes = sendRequest(messageNode);
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::getEncryptionStatus(const QString &jid)
{
    if (q_ptr->m_connectionStatus == WAConnection::LoggedIn) {
        qulonglong recepientId = getRecepient(jid);
        bool encrypted = axolotlStore->containsSession(recepientId, 1) || cipherHash.contains(recepientId);
        Q_EMIT q_ptr->encryptionStatus(jid, encrypted);
    }
}

int WAConnectionPrivate::sendRequest(const ProtocolTreeNode &node)
{
    if (socket->isOpen()) {
        return out->write(node, false);
    }
    return 0;
}

int WAConnectionPrivate::sendRequest(const ProtocolTreeNode &node, const char *member)
{
    if (socket->isOpen()) {
        m_bindStore[node.getAttributeValue("id")] = member;
        return sendRequest(node);
    }
    return 0;
}

void WAConnectionPrivate::tryLogin()
{
    int outBytes, inBytes;
    outBytes = inBytes = 0;
    outBytes = out->streamStart(m_domain, m_resource);
    outBytes += sendFeatures();
    outBytes += sendAuth();

    connect(socket, SIGNAL(readyRead()), this, SLOT(readNode()));

    //counters->increaseCounter(DataCounters::ProtocolBytes, inBytes, outBytes);
}

void WAConnectionPrivate::loginInternal()
{
    connect(socket, SIGNAL(connected()), this, SLOT(socketConnected()));
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(socketError(QAbstractSocket::SocketError)));
    connect(socket, SIGNAL(disconnected()), this, SLOT(socketDisconnected()));

    q_ptr->m_connectionStatus = WAConnection::Connecting;
    Q_EMIT q_ptr->connectionStatusChanged(q_ptr->m_connectionStatus);

    QTime midnight(0,0,0);
    qsrand(midnight.secsTo(QTime::currentTime()));
    QString server = m_servers.at(qrand() % m_servers.size());

    qDebug() << "Connecting to" << server + ":443";
    socket->connectToHost(server, 443);
}

int WAConnectionPrivate::sendFeatures()
{
    ProtocolTreeNode node("stream:features");

    ProtocolTreeNode child1("readreceipts");
    node.addChild(child1);

    ProtocolTreeNode child2("groups_v2");
    node.addChild(child2);

    ProtocolTreeNode child3("privacy");
    node.addChild(child3);

    ProtocolTreeNode child4("presence");
    node.addChild(child4);

    return sendRequest(node);
}

int WAConnectionPrivate::sendAuth()
{
    AttributeList attrs;
    if (m_passive) {
        attrs.insert("passive", "true");
    }
    attrs.insert("mechanism", "WAUTH-2");
    attrs.insert("user", m_username);
    qDebug() << "Sending" << "WAUTH-2" << "for" << m_username;

    ProtocolTreeNode node("auth");
    node.setAttributes(attrs);
    if (m_nextChallenge.size() > 0)
        node.setData(getAuthBlob(m_nextChallenge));

    int bytes = sendRequest(node);
    if (m_nextChallenge.size() > 0)
        out->setCrypto(true);

    return bytes;
}

QByteArray WAConnectionPrivate::getAuthBlob(const QByteArray &nonce)
{
    QList<QByteArray> keys = KeyStream::keyFromPasswordAndNonce(m_password, nonce);
    outputKey = new KeyStream(keys.at(0), keys.at(1), this);
    inputKey = new KeyStream(keys.at(2), keys.at(3), this);

    in->setInputKey(inputKey);
    out->setOutputKey(outputKey);

    QByteArray list(4, '\0');

    list.append(m_username.toUtf8());
    list.append(nonce);

    qint64 totalSeconds = QDateTime::currentMSecsSinceEpoch();
    list.append(QString::number(totalSeconds).toUtf8());
    /*list.append(m_userAgent);
    if (!m_mcc.isEmpty() && !m_mnc.isEmpty()) {
        list.append(QString(" MccMnc/%1%2").arg(m_mcc).arg(m_mnc));
    }*/
    qDebug() << list.mid(4 + m_username.size() + nonce.size());

    outputKey->encodeMessage(list, 0, 4, list.length()-4);

    return list;
}

int WAConnectionPrivate::sendResponse(const QByteArray &challengeData)
{
    QByteArray authBlob = getAuthBlob(challengeData);

    ProtocolTreeNode node("response", authBlob);
    int bytes = sendRequest(node);
    out->setCrypto(true);

    return bytes;
}

void WAConnectionPrivate::parseSuccessNode(const ProtocolTreeNode &node)
{
    AttributeList accountData = node.getAttributes();

    if (node.getAttributeValue("status") == "expired") {
        Q_EMIT q_ptr->accountExpired(accountData);

        logout();
        return;
    }

    uint now = QDateTime::currentDateTime().toTime_t();
    serverTimeCorrection = node.getAttributeValue("t").toUInt() - now;

    m_nextChallenge = node.getData();
    accountData["nextChallenge"] = QString(m_nextChallenge.toBase64());

    if (!m_passive && !m_resource.startsWith("S40") && axolotlStore->countPreKeys() == 0) {
        sendEncrypt();
    }

    Q_EMIT q_ptr->authSuccess(accountData);

    retry = 0;

    sendGetPrivacyList();
    sendGetPushConfig();

    if (m_passive) {
        q_ptr->m_connectionStatus = WAConnection::Initiaization;
        Q_EMIT q_ptr->connectionStatusChanged(q_ptr->m_connectionStatus);
    }
    else {
        sendPing();

        //lastActivity = QDateTime::currentDateTime().toTime_t();

        q_ptr->m_connectionStatus = WAConnection::LoggedIn;
        Q_EMIT q_ptr->connectionStatusChanged(q_ptr->m_connectionStatus);
    }
}

void WAConnectionPrivate::parseContactsNotification(const ProtocolTreeNode &node)
{
    ProtocolTreeNodeListIterator i(node.getChildren());
    while (i.hasNext())
    {
        ProtocolTreeNode child = i.next().value();
        QString tag = child.getTag();
        if (tag == "update") {
            QString jid = child.getAttributeValue("jid");
            sendGetLastSeen(jid);
            sendGetPictureIds(QStringList() << jid);
            sendGetStatuses(QStringList() << jid);
        }
        else {
            Q_EMIT q_ptr->contactsNotification(tag, child.getAttributes());
        }
    }
}

void WAConnectionPrivate::parsePictureNotification(const ProtocolTreeNode &node)
{
    ProtocolTreeNodeListIterator i(node.getChildren());
    while (i.hasNext())
    {
        ProtocolTreeNode child = i.next().value();
        QString tag = child.getTag();
        QString jid = child.getAttributeValue("jid");
        if (tag == "set") {
            Q_EMIT q_ptr->contactPictureId(jid, child.getAttributeValue("id"), child.getAttributeValue("author"));
        }
        else if (tag == "delete") {
            sendGetPicture(jid);
        }
    }
}

void WAConnectionPrivate::parseStatusNotification(const ProtocolTreeNode &node)
{
    if (node.getChildren().contains("set")) {
        QString status = node.getChild("set").getDataString();
        Q_EMIT q_ptr->contactStatus(node.getAttributeValue("from"), status, node.getAttributeValue("t"));
    }
}

void WAConnectionPrivate::parseGroupNotification(const ProtocolTreeNode &node)
{
    QString gjid = node.getAttributeValue("from");
    ProtocolTreeNodeListIterator i(node.getChildren());
    while (i.hasNext())
    {
        ProtocolTreeNode child = i.next().value();
        QString tag = child.getTag();
        if (tag == "add") {
            ProtocolTreeNodeListIterator j(child.getChildren());
            while (j.hasNext())
            {
                ProtocolTreeNode participant = j.next().value();
                if (participant.getTag() == "participant") {
                    Q_EMIT q_ptr->groupParticipantAdded(gjid, participant.getAttributeValue("jid"));
                }
            }
        }
        else if (tag == "remove") {
            ProtocolTreeNodeListIterator j(child.getChildren());
            while (j.hasNext())
            {
                ProtocolTreeNode participant = j.next().value();
                if (participant.getTag() == "participant") {
                    Q_EMIT q_ptr->groupParticipantRemoved(gjid, participant.getAttributeValue("jid"));
                }
            }
        }
        else if (tag == "promote") {
            ProtocolTreeNodeListIterator j(child.getChildren());
            while (j.hasNext())
            {
                ProtocolTreeNode participant = j.next().value();
                if (participant.getTag() == "participant") {
                    Q_EMIT q_ptr->groupParticipantPromoted(gjid, participant.getAttributeValue("jid"));
                }
            }
        }
        else if (tag == "demote") {
            ProtocolTreeNodeListIterator j(child.getChildren());
            while (j.hasNext())
            {
                ProtocolTreeNode participant = j.next().value();
                if (participant.getTag() == "participant") {
                    Q_EMIT q_ptr->groupParticipantDemoted(gjid, participant.getAttributeValue("jid"));
                }
            }
        }
        else if (tag == "subject") {
            Q_EMIT q_ptr->groupSubjectChanged(gjid, child.getAttributeValue("subject"), child.getAttributeValue("s_o"), child.getAttributeValue("s_t"));
        }
        else if (tag == "create") {
            ProtocolTreeNodeListIterator j(child.getChildren());
            while (j.hasNext())
            {
                ProtocolTreeNode group = j.next().value();
                if (group.getTag() == "group") {
                    QStringList participants;
                    QStringList admins;
                    ProtocolTreeNodeListIterator k(group.getChildren());
                    while (k.hasNext())
                    {
                        ProtocolTreeNode participant = k.next().value();
                        if (participant.getTag() == "participant") {
                            QString jid = participant.getAttributeValue("jid");
                            participants.append(jid);
                            if (participant.getAttributeValue("type") == "admin") {
                                admins.append(jid);
                            }
                        }
                    }
                    Q_EMIT q_ptr->groupCreated(gjid,
                                               group.getAttributeValue("creation"),
                                               group.getAttributeValue("creator"),
                                               group.getAttributeValue("s_o"),
                                               group.getAttributeValue("s_t"),
                                               group.getAttributeValue("subject"),
                                               participants,
                                               admins);
                }
            }
        }
    }
}

void WAConnectionPrivate::parseEncryptNotification(const ProtocolTreeNode &node)
{
    if (node.getChildren().contains("count")) {
        ProtocolTreeNode countNode = node.getChild("count");
        int prekeysRemaining = countNode.getAttributeValue("value").toInt();
        if (prekeysRemaining <= 10) {
            sendEncrypt(false);
        }
    }
}

void WAConnectionPrivate::parseReceipt(const ProtocolTreeNode &node)
{
    QString type = node.getAttributeValue("type");
    if (type == "retry") {
        if (node.getChildren().contains("registration")) {
            QString jid = node.getAttributeValue("from");
            qulonglong recepientId = getRecepient(jid);
            axolotlStore->deleteSession(recepientId, 1);
            axolotlStore->removeIdentity(recepientId);
            Q_EMIT q_ptr->retryMessage(node.getAttributeValue("id"), jid);
        }
    }
    else {
        if (node.getChildren().contains("list")) {
            parseReceiptList(node);
        }
        Q_EMIT q_ptr->messageReceipt(node.getAttributeValue("from"), node.getAttributeValue("id"), node.getAttributeValue("participant"), node.getAttributeValue("t"), node.getAttributeValue("type"));
    }
}

void WAConnectionPrivate::parseReceiptList(const ProtocolTreeNode &node)
{
    ProtocolTreeNode listNode = node.getChild("list");
    ProtocolTreeNodeListIterator i(listNode.getChildren());
    while (i.hasNext())
    {
        ProtocolTreeNode itemNode = i.next().value();
        Q_EMIT q_ptr->messageReceipt(node.getAttributeValue("from"), itemNode.getAttributeValue("id"), node.getAttributeValue("participant"), node.getAttributeValue("t"), node.getAttributeValue("type"));
    }
}

void WAConnectionPrivate::parsePresence(const ProtocolTreeNode &node)
{
    QString jid = node.getAttributeValue("from");
    if (node.getAttributes().contains("type")) {
        Q_EMIT q_ptr->contactUnavailable(jid, node.getAttributeValue("last"));
    }
    else {
        Q_EMIT q_ptr->contactAvailable(jid);
    }
}

void WAConnectionPrivate::parseChatstate(const ProtocolTreeNode &node)
{
    QString jid = node.getAttributeValue("from");
    if (node.getChildren().contains("paused")) {
        Q_EMIT q_ptr->contactTypingPaused(jid);
    }
    else if (node.getChildren().contains("composing")) {
        Q_EMIT q_ptr->contactTypingStarted(jid);
    }
}

void WAConnectionPrivate::parseCall(const ProtocolTreeNode &node)
{
    QString jid = node.getAttributeValue("from");
    ProtocolTreeNodeListIterator i(node.getChildren());
    while (i.hasNext())
    {
        ProtocolTreeNode itemNode = i.next().value();
        QString tag = itemNode.getTag();
        if (tag == "offer") {
            sendCallReceipt(node);
            sendCallReject(jid, node.getAttributeValue("id"), itemNode.getAttributeValue("call-id"));
            Q_EMIT q_ptr->incomingCall(jid, node.getAttributeValue("t"));
        }
        else {
            sendCallAck(node);
        }
    }
}

QString WAConnectionPrivate::makeId()
{
    return QString::number(++iqid, 16);
}

QString WAConnectionPrivate::messageId()
{
    QWriteLocker locker(&lockSeq);
    QString msgId = QString("%1-%2").arg(sessionTime).arg(QString::number(mseq++, 16));
    return msgId;
}

void WAConnectionPrivate::sendClientConfig()
{
    AttributeList attrs;
    attrs.insert("id", makeId());
    attrs.insert("type", "set");
    attrs.insert("to", m_domain);
    attrs.insert("xmlns", "urn:xmpp:whatsapp:push");
    ProtocolTreeNode iqNode("iq", attrs);

    ProtocolTreeNode configNode("config");
    iqNode.addChild(configNode);

    int bytes = sendRequest(iqNode);
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendMessageReceived(const QString &jid, const QString &msdId, const QString &type, const QString &participant)
{
    ProtocolTreeNode receiptNode("receipt");
    AttributeList attrs;
    attrs.insert("to", jid);
    attrs.insert("id", msdId);
    //attrs.insert("t", QString::number(QDateTime::currentMSecsSinceEpoch()));
    if (!participant.isEmpty()) {
        attrs.insert("participant", participant);
    }
    if (!type.isEmpty()) {
        attrs.insert("type", type);
    }
    receiptNode.setAttributes(attrs);

    int bytes = sendRequest(receiptNode);
}

void WAConnectionPrivate::sendMessageRetry(const QString &jid, const QString &msdId)
{
    ProtocolTreeNode receiptNode("receipt");
    AttributeList attrs;
    attrs.insert("to", jid);
    attrs.insert("id", msdId);
    attrs.insert("t", QString::number(QDateTime::currentMSecsSinceEpoch()));
    attrs.insert("type", "retry");
    receiptNode.setAttributes(attrs);

    ProtocolTreeNode registrationNode("registration");
    //quint64 registrationId = axolotlStore->getLocalRegistrationId();
    quint64 registrationId = 0;
    registrationNode.setData(QByteArray::fromHex(QByteArray::number(registrationId, 16)).rightJustified(4, '\0'));
    receiptNode.addChild(registrationNode);

    ProtocolTreeNode retryNode("retry");
    attrs.clear();
    attrs.insert("count", "1");
    attrs.insert("id", msdId);
    attrs.insert("t", receiptNode.getAttributeValue("t"));
    attrs.insert("v", "1");
    retryNode.setAttributes(attrs);
    receiptNode.addChild(retryNode);

    int bytes = sendRequest(receiptNode);
}

void WAConnectionPrivate::sendReceiptAck(const ProtocolTreeNode &node)
{
    QString type = node.getAttributeValue("type");

    AttributeList attrs;
    attrs.insert("class", "receipt");
    attrs.insert("type", type.isEmpty() ? "delivery" : type);
    attrs.insert("id", node.getAttributeValue("id"));
    attrs.insert("to", node.getAttributeValue("from"));
    ProtocolTreeNode ackNode("ack", attrs);

    int bytes = sendRequest(ackNode);
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendCleanDirty(const QStringList &categories)
{
    ProtocolTreeNode iqNode("iq");
    AttributeList attrs;
    attrs.insert("id", makeId());
    attrs.insert("type", "set");
    attrs.insert("to", m_domain);
    attrs.insert("xmlns", "urn:xmpp:whatsapp:dirty");
    iqNode.setAttributes(attrs);
    foreach (const QString &category, categories) {
        ProtocolTreeNode catNode("clean");
        attrs.clear();
        attrs.insert("type", category);
        catNode.setAttributes(attrs);
        iqNode.addChild(catNode);
    }

    int bytes = sendRequest(iqNode);
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendNotificationReceived(const ProtocolTreeNode &node)
{
    QString type = node.getAttributeValue("type");
    QString from = node.getAttributeValue("from");
    QString to = node.getAttributeValue("to");
    QString participant = node.getAttributeValue("participant");
    QString id = node.getAttributeValue("id");

    ProtocolTreeNode ackNode("ack");
    AttributeList attrs;
    attrs.insert("to", from);
    attrs.insert("class", "notification");
    attrs.insert("id", id);
    attrs.insert("type", type);
    if (!participant.isEmpty()) {
        attrs.insert("participant", participant);
    }
    if (!to.isEmpty()) {
        attrs.insert("from", to);
    }
    ackNode.setAttributes(attrs);

    if (type == "contacts") {
        ProtocolTreeNode sync("sync");
        AttributeList syncattrs;
        syncattrs.insert("contacts", "out");
        sync.setAttributes(syncattrs);

        ackNode.addChild(sync);
    }

    int bytes = sendRequest(ackNode);
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendResult(const QString &id)
{
    ProtocolTreeNode iqNode("iq");
    AttributeList attrs;
    attrs.insert("id", id);
    attrs.insert("type", "result");
    attrs.insert("to", m_domain);
    iqNode.setAttributes(attrs);

    int bytes = sendRequest(iqNode);
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendPassive(const QString &mode)
{
    ProtocolTreeNode iqNode("iq");
    AttributeList attrs;
    attrs.insert("id", makeId());
    attrs.insert("type", "set");
    attrs.insert("xmlns", "passive");
    attrs.insert("to", m_domain);
    iqNode.setAttributes(attrs);

    ProtocolTreeNode modeNode(mode);
    iqNode.addChild(modeNode);

    int bytes = sendRequest(iqNode, WAREPLY(passiveResponse));
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendCallReceipt(const ProtocolTreeNode &node)
{
/*
    ProtocolTreeNode receiptNode("receipt");
    AttributeList attrs;
    attrs.insert("to", node.getAttributeValue("from"));
    attrs.insert("id", node.getAttributeValue("id"));
    receiptNode.setAttributes(attrs);

    ProtocolTreeNode callNode = node.getChildren().first();

    ProtocolTreeNode actionNode(callNode.getTag());
    attrs.clear();
    attrs.insert("call-id", callNode.getAttributeValue("call-id"));
    actionNode.setAttributes(attrs);

    receiptNode.addChild(actionNode);

    int bytes = sendRequest(receiptNode);
*/
}

void WAConnectionPrivate::sendCallAck(const ProtocolTreeNode &node)
{
/*
    ProtocolTreeNode callNode = node.getChildren().first();

    ProtocolTreeNode ackNode("ack");
    AttributeList attrs;
    attrs.insert("to", node.getAttributeValue("from"));
    attrs.insert("class", "call");
    attrs.insert("id", node.getAttributeValue("id"));
    attrs.insert("type", callNode.getTag());
    ackNode.setAttributes(attrs);

    int bytes = sendRequest(ackNode);
*/
}

void WAConnectionPrivate::sendCallReject(const QString &jid, const QString &id, const QString &callId)
{
    ProtocolTreeNode callNode("call");
    AttributeList attrs;
    attrs.insert("to", jid);
    attrs.insert("id", id);
    callNode.setAttributes(attrs);

    ProtocolTreeNode rejectNode("reject");
    attrs.clear();
    attrs.insert("call-id", callId);
    rejectNode.setAttributes(attrs);

    callNode.addChild(rejectNode);

    int bytes = sendRequest(callNode);
}

bool WAConnectionPrivate::parseMessage(const ProtocolTreeNode &node)
{
    QString jid = node.getAttributeValue("from");
    QString id = node.getAttributeValue("id");
    QString timestamp = node.getAttributeValue("t");
    QString type = node.getAttributeValue("type");
    if (type == "text") {
        if (node.getChildren().contains("body")) {
            ProtocolTreeNode bodyNode = node.getChild("body");
            Q_EMIT q_ptr->textMessageReceived(jid, id, timestamp, node.getAttributeValue("participant"), node.getAttributes().contains("offline"), bodyNode.getDataString());
        }
        else if (node.getChildren().contains("enc")) {
            return parseEncryptedMessage(node);
        }
    }
    else if (type == "media") {
        if (node.getChildren().contains("media")) {
            ProtocolTreeNode mediaNode = node.getChild("media");
            AttributeList attrs = mediaNode.getAttributes();
            QByteArray data;
            if (mediaNode.getAttributeValue("type") == "vcard") {
                ProtocolTreeNode vcardNode = mediaNode.getChild("vcard");
                attrs["name"] = vcardNode.getAttributeValue("name");
                data = vcardNode.getData();
            }
            else {
                data = mediaNode.getData();
            }
            Q_EMIT q_ptr->mediaMessageReceived(jid, id, timestamp, node.getAttributeValue("participant"), node.getAttributes().contains("offline"), attrs, data);
        }
    }
    return true;
}

bool WAConnectionPrivate::parseEncryptedMessage(const ProtocolTreeNode &node)
{
    ProtocolTreeNode encNode = node.getChild("enc");
    if (encNode.getAttributeValue("type") == "pkmsg") {
        return parsePreKeyWhisperMessage(node);
    }
    else {
        return parseWhisperMessage(node);
    }
}

bool WAConnectionPrivate::parsePreKeyWhisperMessage(const ProtocolTreeNode &node)
{
    QString jid = node.getAttributeValue("from");
    QString id = node.getAttributeValue("id");
    QString timestamp = node.getAttributeValue("t");

    ProtocolTreeNode encNode = node.getChild("enc");
    try {
        QSharedPointer<PreKeyWhisperMessage> message(new PreKeyWhisperMessage(encNode.getData()));

        qulonglong recepientId = getRecepient(jid);
        SessionCipher *cipher = getSessionCipher(recepientId);
        QString plaintext = cipher->decrypt(message);

        qDebug() << "DECRYPTED:" << plaintext;
        Q_EMIT q_ptr->textMessageReceived(jid, id, timestamp, node.getAttributeValue("participant"), node.getAttributes().contains("offline"), plaintext);
    }
    catch (WhisperException &e) {
        qWarning() << "EXCEPTION" << e.errorType() << e.errorMessage();
        sendMessageRetry(jid, id);
        return false;
    }
    return true;
}

bool WAConnectionPrivate::parseWhisperMessage(const ProtocolTreeNode &node)
{
    QString jid = node.getAttributeValue("from");
    QString id = node.getAttributeValue("id");
    QString timestamp = node.getAttributeValue("t");

    ProtocolTreeNode encNode = node.getChild("enc");
    try {
        QSharedPointer<WhisperMessage> message(new WhisperMessage(encNode.getData()));

        qulonglong recepientId = getRecepient(jid);
        SessionCipher *cipher = getSessionCipher(recepientId);
        QString plaintext = cipher->decrypt(message);

        qDebug() << "DECRYPTED:" << plaintext;
        Q_EMIT q_ptr->textMessageReceived(jid, id, timestamp, node.getAttributeValue("participant"), node.getAttributes().contains("offline"), plaintext);
    }
    catch (WhisperException &e) {
        qWarning() << "EXCEPTION" << e.errorType() << e.errorMessage();
        if (e.errorType() == "NoSessionException") {
            //axolotlStore.clear();
            //sendEncrypt();
        }
        sendMessageRetry(jid, id);
        return false;
    }
    return true;
}

SessionCipher *WAConnectionPrivate::getSessionCipher(qulonglong recepient)
{
    if (cipherHash.contains(recepient)) {
        return cipherHash.value(recepient);
    }
    else {
        SessionCipher *cipher = new SessionCipher(axolotlStore, recepient, 1);
        cipherHash[recepient] = cipher;
        return cipher;
    }
}

ProtocolTreeNode WAConnectionPrivate::getTextBody(const QString &text)
{
    ProtocolTreeNode bodyNode("body", text.toUtf8());
    return bodyNode;
}

ProtocolTreeNode WAConnectionPrivate::getBroadcastNode(const QStringList &jids)
{
    ProtocolTreeNode broadcastNode("broadcast");
    foreach (const QString &jid, jids) {
        ProtocolTreeNode broadcastChild("to");
        AttributeList to;
        to.insert("jid", jid);
        broadcastChild.setAttributes(to);
        broadcastNode.addChild(broadcastChild);
    }
    return broadcastNode;
}

ProtocolTreeNode WAConnectionPrivate::getMessageNode(const QString &jid, const QString &type, const QString &msgId)
{
    ProtocolTreeNode messageNode("message");
    AttributeList attrs;
    attrs.insert("id", msgId.isEmpty() ? messageId() : msgId);
    attrs.insert("type", type);
    attrs.insert("to", jid);
    messageNode.setAttributes(attrs);
    return messageNode;
}

void WAConnectionPrivate::sendEncrypt(bool fresh)
{
    qDebug() << "Generating keys...";

    IdentityKeyPair identityKeyPair = fresh ? KeyHelper::generateIdentityKeyPair() : axolotlStore->getIdentityKeyPair();
    quint64 registrationId          = fresh ? KeyHelper::generateRegistrationId() : axolotlStore->getLocalRegistrationId();
    QList<PreKeyRecord> preKeys     = KeyHelper::generatePreKeys(KeyHelper::getRandomFFFFFFFF(), 200);
    SignedPreKeyRecord signedPreKey = KeyHelper::generateSignedPreKey(identityKeyPair, 0);

    ProtocolTreeNode iqNode("iq");
    AttributeList attrs;
    attrs.insert("xmlns", "encrypt");
    attrs.insert("to", m_domain);
    attrs.insert("type", "set");
    attrs.insert("id", makeId());
    iqNode.setAttributes(attrs);

    ProtocolTreeNode identityNode("identity");
    identityNode.setData(identityKeyPair.getPublicKey().serialize().mid(1));

    // STORE
    if (fresh) {
        axolotlStore->storeLocalData(registrationId, identityKeyPair);
    }

    ProtocolTreeNode listNode("list");
    QListIterator<PreKeyRecord> iter(preKeys);
    while (iter.hasNext()) {
        PreKeyRecord preKey = iter.next();
        ProtocolTreeNode keyNode("key");
        ProtocolTreeNode idNode("id");
        QByteArray keyId = QByteArray::number(preKey.getId(), 16);
        keyId = QByteArray::fromHex(keyId).rightJustified(3, '\0');
        idNode.setData(keyId);
        ProtocolTreeNode valueNode("value");
        valueNode.setData(preKey.getKeyPair().getPublicKey().serialize().mid(1));
        keyNode.addChild(idNode);
        keyNode.addChild(valueNode);
        listNode.addChild(keyNode);

        // STORE
        axolotlStore->storePreKey(preKey.getId(), preKey);
    }

    ProtocolTreeNode registrationNode("registration");
    registrationNode.setData(QByteArray::fromHex(QByteArray::number(registrationId, 16)).rightJustified(4, '\0'));

    ProtocolTreeNode typeNode("type");
    typeNode.setData(QByteArray(1, '\5'));

    ProtocolTreeNode skeyNode("skey");
    ProtocolTreeNode idNode("id");
    QByteArray keyId = QByteArray::number(signedPreKey.getId(), 16);
    keyId = QByteArray::fromHex(keyId).rightJustified(3, '\0');
    idNode.setData(keyId);
    ProtocolTreeNode valueNode("value");
    valueNode.setData(signedPreKey.getKeyPair().getPublicKey().serialize().mid(1));
    ProtocolTreeNode signatureNode("signature");
    signatureNode.setData(signedPreKey.getSignature());
    skeyNode.addChild(idNode);
    skeyNode.addChild(valueNode);
    skeyNode.addChild(signatureNode);

    // STORE
    axolotlStore->storeSignedPreKey(signedPreKey.getId(), signedPreKey);

    iqNode.addChild(identityNode);
    iqNode.addChild(listNode);
    iqNode.addChild(registrationNode);
    iqNode.addChild(typeNode);
    iqNode.addChild(skeyNode);

    int bytes = sendRequest(iqNode, WAREPLY(encryptionReply));
}

void WAConnectionPrivate::sendGetEncryptKeys(const QStringList &jids)
{
    ProtocolTreeNode iqNode("iq");
    AttributeList attrs;
    attrs.insert("id", makeId());
    attrs.insert("to", m_domain);
    attrs.insert("type", "get");
    attrs.insert("xmlns", "encrypt");
    iqNode.setAttributes(attrs);

    ProtocolTreeNode keyNode("key");

    foreach (const QString &jid, jids) {
        ProtocolTreeNode userNode("user");
        AttributeList userAttrs;
        userAttrs.insert("jid", jid);
        userNode.setAttributes(userAttrs);
        keyNode.addChild(userNode);
    }

    iqNode.addChild(keyNode);

    int bytes = sendRequest(iqNode, WAREPLY(getKeysReponse));
}

qulonglong WAConnectionPrivate::getRecepient(const QString &jid)
{
    return jid.split("@").first().toULongLong();
}

bool WAConnectionPrivate::read()
{
    ProtocolTreeNode node;

    if (!m_isReading) {
        m_isReading = true;
    }
    else {
        return true;
    }

    if (in->nextTree(node))
    {
        bool handled = false;

        QString id = node.getAttributeValue("id");
        if (m_bindStore.contains(id)) {
            QMetaObject::invokeMethod(this, m_bindStore.take(id), Q_ARG(ProtocolTreeNode, node));
            handled = true;
        }
        else {
            QString tag = node.getTag();

            if (tag == "stream:start")
            {
                handled = true;
            }
            else if (tag == "stream:close")
            {
                handled = true;
            }
            else if (tag == "stream:features")
            {
                handled = true;
            }
            else if (tag == "stream:error")
            {
                qDebug() << "STREAM_ERROR!";
                ProtocolTreeNodeListIterator i(node.getChildren());
                while (i.hasNext())
                {
                    ProtocolTreeNode child = i.next().value();
                    qDebug() << child.getTag() << child.getDataString();
                }
                Q_EMIT q_ptr->streamError();
                handled = true;
            }
            else if (tag == "challenge")
            {
                m_nextChallenge = node.getData();
                sendResponse(m_nextChallenge);
                handled = true;
            }
            else if (tag == "success")
            {
                parseSuccessNode(node);
                handled = true;
            }
            else if (tag == "failure")
            {
                Q_EMIT q_ptr->authFailed();
                m_authFailed = true;
                handled = true;
            }
            else if (tag == "message")
            {
                if (parseMessage(node)) {
                    sendMessageReceived(node.getAttributeValue("from"), node.getAttributeValue("id"), QString(), node.getAttributeValue("participant"));
                }
                handled = true;
            }
            else if (tag == "iq")
            {
                QString xmlns = node.getAttributeValue("xmlns");
                QString id = node.getAttributeValue("id");
                if (xmlns == "urn:xmpp:ping") {
                    sendResult(id);
                    handled = true;
                }
            }
            else if (tag == "notification")
            {
                sendNotificationReceived(node);
                QString type = node.getAttributeValue("type");
                if (type == "contacts") {
                    parseContactsNotification(node);
                }
                else if (type == "picture") {
                    parsePictureNotification(node);
                }
                else if (type == "w:gp2") {
                    parseGroupNotification(node);
                }
                else if (type == "status") {
                    parseStatusNotification(node);
                }
                else if (type == "encrypt") {
                    parseEncryptNotification(node);
                }
                handled = true;
            }
            else if (tag == "receipt")
            {
                parseReceipt(node);
                sendReceiptAck(node);
                handled = true;
            }
            else if (tag == "ib")
            {
                ProtocolTreeNodeListIterator i(node.getChildren());
                while (i.hasNext()) {
                    ProtocolTreeNode child = i.next().value();
                    if (child.getTag() == "dirty") {
                        sendCleanDirty(QStringList() << child.getAttributeValue("type"));
                        handled = true;
                    }
                    else if (child.getTag() == "offline") {
                        Q_EMIT q_ptr->notifyOfflineMessages(child.getAttributeValue("count").toInt());
                        handled = true;
                    }
                }
            }
            else if (tag == "ack")
            {
                handled = true;
            }
            else if (tag == "presence")
            {
                parsePresence(node);
                handled = true;
            }
            else if (tag == "chatstate")
            {
                parseChatstate(node);
                handled = true;
            }
            else if (tag == "call") {
                parseCall(node);
                handled = true;
            }
        }
        if (!handled) {
            qDebug() << "TODO: Unhandled node!";
        }
        if (node.getAttributes().contains("notify")) {
            QString notify = node.getAttributeValue("notify");
            QString user = node.getAttributes().contains("participant") ? node.getAttributeValue("participant") : node.getAttributeValue("from");
            Q_EMIT q_ptr->notifyPushname(user, notify);
        }

        m_isReading = false;

        return true;
    }

    return false;
}

void WAConnectionPrivate::socketConnected()
{
    socketLastError = QAbstractSocket::UnknownSocketError;

    qDebug() << "connected";
    socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    q_ptr->m_connectionStatus = WAConnection::Connected;
    Q_EMIT q_ptr->connectionStatusChanged(q_ptr->m_connectionStatus);

    tryLogin();
}

void WAConnectionPrivate::socketDisconnected()
{
    qDebug() << "disconnected" << socketLastError;
    QObject::disconnect(socket, 0, 0, 0);
    out->reset();
    in->reset();
    iqid = 0;
    mseq = 0;
    cipherHash.clear();

    if (socketLastError == QTcpSocket::RemoteHostClosedError) {
        m_nextChallenge.clear();
        int maxRetry = 10;
        if (!m_authFailed && retry < maxRetry) {
            q_ptr->m_connectionStatus = WAConnection::Connecting;
            Q_EMIT q_ptr->connectionStatusChanged(q_ptr->m_connectionStatus);

            retry++;
            qDebug() << QString("Retry login in %1 seconds... [%2/%3]").arg(retry).arg(retry).arg(maxRetry);
            QTimer::singleShot(retry * 1000, this, SLOT(loginInternal()));
        }
        else {
            q_ptr->m_connectionStatus = WAConnection::Disconnected;
            Q_EMIT q_ptr->connectionStatusChanged(q_ptr->m_connectionStatus);
        }
    }
    else if (m_passiveReconnect) {
        m_passiveReconnect = false;
        QTimer::singleShot(1000, this, SLOT(loginInternal()));
    }
    else {
        q_ptr->m_connectionStatus = WAConnection::Disconnected;
        Q_EMIT q_ptr->connectionStatusChanged(q_ptr->m_connectionStatus);
    }
}

void WAConnectionPrivate::socketError(QAbstractSocket::SocketError error)
{
    qDebug() << "error:" << error << "isOpen" << socket->isOpen();
    socketLastError = error;

    if (error == QTcpSocket::NetworkError) {
        q_ptr->m_connectionStatus = WAConnection::Disconnected;
        Q_EMIT q_ptr->connectionStatusChanged(q_ptr->m_connectionStatus);
    }
}

void WAConnectionPrivate::connectionServerProperties(const ProtocolTreeNode &node)
{
    qDebug() << "Received server properties";
    QVariantMap props;
    ProtocolTreeNode propsNode = node.getChild("props");
    ProtocolTreeNodeListIterator i(propsNode.getChildren());
    while (i.hasNext())
    {
        ProtocolTreeNode child = i.next().value();
        if (child.getTag() == "prop") {
            props.insert(child.getAttributeValue("name"), child.getAttributeValue("value"));
        }
    }
    Q_EMIT q_ptr->serverProperties(props);
}

void WAConnectionPrivate::contactLastSeen(const ProtocolTreeNode &node)
{
    QString jid = node.getAttributeValue("from");
    if (node.getChildren().contains("query")) {
        ProtocolTreeNode query = node.getChild("query");
        uint seconds = query.getAttributeValue("seconds").toUInt();
        uint timestamp = QDateTime::currentDateTime().toTime_t() - seconds;
        Q_EMIT q_ptr->contactLastSeen(jid, timestamp);
    }
    else if (node.getChildren().contains("error")) {
        ProtocolTreeNode error = node.getChild("error");
        Q_EMIT q_ptr->contactLastSeenHidden(jid, error.getAttributeValue("code"));
    }
}

void WAConnectionPrivate::contactsStatuses(const ProtocolTreeNode &node)
{
    if (node.getChildren().contains("status")) {
        QVariantMap result;
        ProtocolTreeNode status = node.getChild("status");
        ProtocolTreeNodeListIterator i(status.getChildren());
        while (i.hasNext())
        {
            ProtocolTreeNode child = i.next().value();
            if (child.getTag() == "user") {
                QString jid = child.getAttributeValue("jid");
                QVariantMap data;
                if (child.getAttributes().contains("type")) {
                    QString type = child.getAttributeValue("type");
                    data["type"] = type;
                    if (type == "fail") {
                        data["status"] = tr("Contact status hidden");
                    }
                    else {
                        //
                    }
                }
                else {
                    data["status"] = child.getDataString();
                    data["t"] = child.getAttributeValue("t");
                }
                result[jid] = data;
            }
        }
        if (result.size() > 0) {
            Q_EMIT q_ptr->contactsStatuses(result);
        }
    }

    if (m_passive) {
        m_passiveCount -= 1;
        if (m_passiveCount == 0) {
            m_passiveCount += 1;
            sendGetGroups("participating");
        }
    }
}

void WAConnectionPrivate::contactPicture(const ProtocolTreeNode &node)
{
    QString jid = node.getAttributeValue("from");
    QString type = node.getAttributeValue("type");
    if (type == "result") {
        if (node.getChildren().contains("picture")) {
            ProtocolTreeNode picture = node.getChild("picture");
            Q_EMIT q_ptr->contactPicture(jid, picture.getData(), picture.getAttributeValue("id"));
        }
    }
    else if (type == "error") {
        if (node.getChildren().contains("error")) {
            ProtocolTreeNode error = node.getChild("error");
            Q_EMIT q_ptr->contactPictureHidden(jid, error.getAttributeValue("code"));
        }
    }

    if (m_passive) {
        m_passiveCount -= 1;
        if (m_passiveCount == 0) {
            m_passiveCount += 1;
            if (!m_passiveGroups) {
                sendGetGroups("participating");
            }
            else {
                sendGetBroadcasts();
            }
        }
    }
}

void WAConnectionPrivate::contactsPicrureIds(const ProtocolTreeNode &node)
{
    if (node.getChildren().contains("list")) {
        QVariantMap contacts;
        ProtocolTreeNode list = node.getChild("list");
        ProtocolTreeNodeListIterator i(list.getChildren());
        QStringList syncPictures;
        while (i.hasNext())
        {
            ProtocolTreeNode child = i.next().value();
            if (child.getTag() == "user") {
                QString jid = child.getAttributeValue("jid");
                if (child.getAttributes().contains("id")) {
                    //Q_EMIT q_ptr->contactPictureId(jid, child.getAttributeValue("id"), jid);
                    contacts[jid] = child.getAttributeValue("id");
                    if (m_passive) {
                        syncPictures.append(jid);
                    }
                }
                else {
                    syncPictures.append(jid);
                    //contacts[jid] == "hidden";
                }
            }
        }
        if (m_passive) {
            m_passiveCount += syncPictures.size();
        }
        foreach (const QString &jid, syncPictures) {
            sendGetPicture(jid);
        }
        if (!m_passive && contacts.size() > 0) {
            Q_EMIT q_ptr->contactsPictureIds(contacts);
        }
    }

    if (m_passive) {
        m_passiveCount -= 1;
        if (m_passiveCount == 0) {
            m_passiveCount += 1;
            if (!m_passiveGroups) {
                sendGetGroups("participating");
            }
            else {
                sendGetBroadcasts();
            }
        }
    }
}

void WAConnectionPrivate::syncResponse(const ProtocolTreeNode &node)
{
    QVariantMap synced;
    ProtocolTreeNode syncNode = node.getChild("sync");
    if (syncNode.getChildren().contains("in")) {
        ProtocolTreeNode inNode = syncNode.getChild("in");
        ProtocolTreeNodeListIterator i(inNode.getChildren());
        while (i.hasNext())
        {
            ProtocolTreeNode userNode = i.next().value();
            if (userNode.getTag() == "user") {
                QString number = userNode.getDataString();
                QString jid = userNode.getAttributeValue("jid");
                if (!synced.contains(jid)) {
                    synced[jid] = syncing["p" + number];
                }
            }
        }
    }
    if (!synced.isEmpty()) {
        //qDebug() << synced;
        if (m_passive) {
            m_passiveCount = 2;
            m_passiveGroups = false;
        }
        Q_EMIT q_ptr->contactsSynced(synced);
    }
    if (m_passive) {
        m_passiveCount += 1;
        m_passiveGroups = false;
    }
    sendGetFeatures(synced.keys());
}

void WAConnectionPrivate::groupsResponse(const ProtocolTreeNode &node)
{
    if (node.getChildren().contains("groups")) {
        QVariantMap groups;
        ProtocolTreeNode groupsNode = node.getChild("groups");
        ProtocolTreeNodeListIterator i(groupsNode.getChildren());
        while (i.hasNext())
        {
            QStringList participants;
            QStringList admins;
            ProtocolTreeNode groupNode = i.next().value();
            ProtocolTreeNodeListIterator j(groupNode.getChildren());
            while (j.hasNext())
            {
                ProtocolTreeNode participantNode = j.next().value();
                QString jid = participantNode.getAttributeValue("jid");
                participants.append(jid);
                if (participantNode.getAttributeValue("type") == "admin") {
                    admins.append(jid);
                }
            }
            QVariantMap group;
            group["creation"] = groupNode.getAttributeValue("creation");
            group["creator"] = groupNode.getAttributeValue("creator");
            group["s_o"] = groupNode.getAttributeValue("s_o");
            group["s_t"] = groupNode.getAttributeValue("s_t");
            group["subject"] = groupNode.getAttributeValue("subject");
            group["participants"] = participants;
            group["admins"] = admins;
            groups[groupNode.getAttributeValue("id") + "@g.us"] = group;
        }
        if (groups.size() > 0) {
            Q_EMIT q_ptr->groupsReceived(groups);

            if (m_passive) {
                m_passiveCount += 1;
                m_passiveGroups = true;
            }
            sendGetPictureIds(groups.keys());
        }
    }

    if (m_passive) {
        m_passiveCount -= 1;
        if (m_passiveCount == 0) {
            sendGetBroadcasts();
        }
    }
}

void WAConnectionPrivate::broadcastsResponse(const ProtocolTreeNode &node)
{
    if (node.getChildren().contains("lists")) {
        QVariantMap broadcasts;
        ProtocolTreeNode listsNode = node.getChild("lists");
        ProtocolTreeNodeListIterator i(listsNode.getChildren());
        while (i.hasNext())
        {
            ProtocolTreeNode listNode = i.next().value();
            QString jid = listNode.getAttributeValue("id");
            QStringList recepients;
            ProtocolTreeNodeListIterator j(listNode.getChildren());
            while (j.hasNext())
            {
                ProtocolTreeNode recepientNode = j.next().value();
                recepients.append(recepientNode.getAttributeValue("jid"));
            }
            QVariantMap broadcast;
            broadcast["jid"] = jid;
            broadcast["name"] = listNode.getAttributeValue("name");
            broadcast["recepients"] = recepients;
            broadcasts[jid] = broadcast;
        }

        if (broadcasts.size() > 0) {
            Q_EMIT q_ptr->broadcastsReceived(broadcasts);
        }
    }

    if (m_passive) {
        sendPing();
        sendPassive("active");
    }
}

void WAConnectionPrivate::passiveResponse(const ProtocolTreeNode &node)
{
    m_passive = false;
    m_passiveCount = 0;
    m_passiveReconnect = true;
    logout();
}

void WAConnectionPrivate::encryptionReply(const ProtocolTreeNode &node)
{
    if (node.getAttributeValue("type") == "error") {
        axolotlStore->clear();
        qWarning() << "Keys are not accepted!";
        logout();
    }
}

void WAConnectionPrivate::onPong(const ProtocolTreeNode &node)
{
    Q_UNUSED(node)
}

void WAConnectionPrivate::getKeysReponse(const ProtocolTreeNode &node)
{
    if (node.getChildren().contains("list")) {
        ProtocolTreeNode listNode = node.getChild("list");
        ProtocolTreeNodeListIterator i(listNode.getChildren());
        while (i.hasNext())
        {
            ProtocolTreeNode userNode = i.next().value();
            if (userNode.getTag() == "user") {
                QString jid = userNode.getAttributeValue("jid");
                Q_EMIT q_ptr->encryptionStatus(jid, true);

                if (pendingMessages.contains(jid)) {
                    QString text = pendingMessages.take(jid);

                    bool ok;

                    ProtocolTreeNode identityNode = userNode.getChild("identity");
                    IdentityKey identityKey(DjbECPublicKey(identityNode.getData()));

                    ProtocolTreeNode keyNode = userNode.getChild("key");
                    ProtocolTreeNode keyIdNode = keyNode.getChild("id");
                    qulonglong preKeyId = keyIdNode.getData().toHex().toULongLong(&ok, 16);
                    ProtocolTreeNode keyValueNode = keyNode.getChild("value");
                    DjbECPublicKey preKeyPublic(keyValueNode.getData());

                    ProtocolTreeNode registrationNode = userNode.getChild("registration");
                    qulonglong registrationId = registrationNode.getData().toHex().toULongLong(&ok, 16);

                    ProtocolTreeNode skeyNode = userNode.getChild("skey");
                    ProtocolTreeNode skeyIdNode = skeyNode.getChild("id");
                    qulonglong skeyId = skeyIdNode.getData().toHex().toULongLong(&ok, 16);
                    ProtocolTreeNode skeySignatureNode = skeyNode.getChild("signature");
                    QByteArray signedSignature = skeySignatureNode.getData();
                    ProtocolTreeNode skeyValueNode = skeyNode.getChild("value");
                    DjbECPublicKey signedKey(skeyValueNode.getData());

                    //ProtocolTreeNode typeNode = userNode.getChild("type");
                    //qulonglong type = typeNode.getData().toHex().toULongLong(&ok, 16);

                    PreKeyBundle bundle(registrationId, 1, preKeyId, preKeyPublic, skeyId, signedKey, signedSignature, identityKey);

                    qulonglong recepientId = getRecepient(jid);
                    SessionBuilder *sessionBuilder = new SessionBuilder(axolotlStore, recepientId, 1);
                    try {
                        sessionBuilder->process(bundle);
                        sendText(jid, text);
                    }
                    catch (WhisperException &e) {
                        qWarning() << "EXCEPTION" << e.errorType() << e.errorMessage();
                        if (e.errorType() == "UntrustedIdentityException") {
                            axolotlStore->removeIdentity(recepientId);
                            pendingMessages[jid] = text;
                        }
                        else {
                            skipEncodingJids.append(jid);
                            Q_EMIT q_ptr->encryptionStatus(jid, false);
                        }
                        sendText(jid, text);
                    }
                }
            }
        }
    }

    if (pendingMessages.count() > 0) {
        QStringList jids = pendingMessages.keys();
        foreach (const QString &jid, jids) {
            skipEncodingJids.append(jid);

            QString text = pendingMessages.take(jid);
            sendText(jid, text);
        }
    }
}

void WAConnectionPrivate::messageSent(const ProtocolTreeNode &node)
{
    Q_EMIT q_ptr->messageSent(node.getAttributeValue("from"), node.getAttributeValue("id"), node.getAttributeValue("t"));
}

void WAConnectionPrivate::featureResponse(const ProtocolTreeNode &node)
{
    if (m_passive) {
        m_passiveCount -= 1;
        if (m_passiveCount == 0) {
            m_passiveCount += 1;
            sendGetGroups("participating");
        }
    }
}

void WAConnectionPrivate::privacyList(const ProtocolTreeNode &node)
{
    if (node.getChildren().contains("query")) {
        ProtocolTreeNode queryNode = node.getChild("query");
        if (queryNode.getChildren().contains("list")) {
            blacklist.clear();

            ProtocolTreeNode listNode = queryNode.getChild("list");
            ProtocolTreeNodeListIterator i(listNode.getChildren());
            while (i.hasNext())
            {
                ProtocolTreeNode itemNode = i.next().value();
                blacklist.append(itemNode.getAttributeValue("value"));
            }

            Q_EMIT q_ptr->blacklistReceived(blacklist);
        }
    }
}

void WAConnectionPrivate::pushConfig(const ProtocolTreeNode &node)
{

}

WAConnection::WAConnection(QObject *parent) :
    QObject(parent),
    d_ptr(new WAConnectionPrivate(this))
{
    qRegisterMetaType<AttributeList>("AttributeList");
}

WAConnection::~WAConnection()
{
}

WAConnection *WAConnection::GetInstance(QObject *parent)
{
    static WAConnection* lsSingleton = NULL;
    if (!lsSingleton) {
        lsSingleton = new WAConnection(parent);
    }
    return lsSingleton;
}

void WAConnection::init()
{
    d_ptr->init();
}

int WAConnection::getConnectionStatus()
{
    return m_connectionStatus;
}

void WAConnection::login(const QVariantMap &loginData)
{
    d_ptr->login(loginData);
}

void WAConnection::logout()
{
    d_ptr->logout();
}

void WAConnection::reconnect()
{
    d_ptr->reconnect();
}

void WAConnection::sendGetProperties()
{
    d_ptr->sendGetProperties();
}

void WAConnection::sendPing()
{
    d_ptr->sendPing();
}

void WAConnection::sendAvailable(const QString &pushname)
{
    d_ptr->sendSetPresence(true, pushname);
}

void WAConnection::sendUnavailable(const QString &pushname)
{
    d_ptr->sendSetPresence(false, pushname);
}

void WAConnection::getEncryptionStatus(const QString &jid)
{
    d_ptr->getEncryptionStatus(jid);
}

void WAConnection::sendTyping(const QString &jid, bool typing)
{
    d_ptr->sendTyping(jid, typing);
}

void WAConnection::sendSetStatusMessage(const QString &message)
{
    d_ptr->sendSetStatusMessage(message);
}

void WAConnection::sendRetryMessage(const QString &jid, const QString &msgId, const QString &data)
{
    d_ptr->sendRetryMessage(jid, msgId, data);
}

void WAConnection::sendGetGroups(const QString &type)
{
    d_ptr->sendGetGroups(type);
}

void WAConnection::sendGetBroadcasts()
{
    d_ptr->sendGetBroadcasts();
}

void WAConnection::sendSubscribe(const QString &jid)
{
    d_ptr->sendPresenceRequest(jid, QString("subscribe"));
}

void WAConnection::sendUnsubscribe(const QString &jid)
{
    d_ptr->sendPresenceRequest(jid, QString("unsubscribe"));
}

void WAConnection::sendGetLastSeen(const QString &jid)
{
    d_ptr->sendGetLastSeen(jid);
}

void WAConnection::sendGetPicture(const QString &jid)
{
    d_ptr->sendGetPicture(jid);
}

void WAConnection::sendGetPictureIds(const QStringList &jids)
{
    d_ptr->sendGetPictureIds(jids);
}

void WAConnection::sendGetStatuses(const QStringList &jids)
{
    d_ptr->sendGetStatuses(jids);
}

void WAConnection::syncContacts(const QVariantMap &contacts)
{
    d_ptr->syncContacts(contacts);
}

void WAConnection::sendMessageRead(const QString &jid, const QString &msgId, const QString &participant)
{
    d_ptr->sendMessageRead(jid, msgId, participant);
}

void WAConnection::sendText(const QString &jid, const QString &text)
{
    d_ptr->sendText(jid, text);
}

void WAConnection::sendBroadcastText(const QString &jid, const QString &text, const QStringList &jids)
{
    d_ptr->sendBroadcastText(jid, text, jids);
}

int WAConnection::sendRequest(const ProtocolTreeNode &node)
{
    return d_ptr->sendRequest(node);
}

int WAConnection::sendRequest(const ProtocolTreeNode &node, const char *member)
{
    return d_ptr->sendRequest(node, member);
}
