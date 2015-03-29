#include "waconnection.h"
#include "waconnection_p.h"
#include "waconstants.h"
#include "protocoltreenodelistiterator.h"

#include <QDateTime>
#include <QMetaObject>
#include <QThread>

WAConnectionPrivate::WAConnectionPrivate(WAConnection *q):
    QObject(q),
    q_ptr(q),
    m_isReading(false)
{
}

void WAConnectionPrivate::init()
{
    socket = new QTcpSocket(this);
    dict = new WATokenDictionary(this);
    out = new BinTreeNodeWriter(socket, dict, this);
    in = new BinTreeNodeReader(socket, dict, this);
    iqid = 0;
    mseq = 0;

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
    m_username = loginData["login"].toString();
    m_password = loginData["password"].toByteArray();
    m_password = QByteArray::fromBase64(m_password);
    m_resource = loginData["resource"].toString();
    m_userAgent = loginData["userAgent"].toString();
    m_mcc = loginData["mcc"].toString();
    m_mnc = loginData["mnc"].toString();
    m_nextChallenge = loginData["nextChallenge"].toByteArray();
    if (m_nextChallenge.size() > 0)
        m_nextChallenge = QByteArray::fromBase64(m_nextChallenge);
    m_domain = QString(JID_DOMAIN);

    connect(socket, SIGNAL(connected()), this, SLOT(socketConnected()));
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(socketError(QAbstractSocket::SocketError)));
    connect(socket, SIGNAL(disconnected()), this, SLOT(socketDisconnected()));

    q_ptr->m_connectionStatus = WAConnection::Connecting;
    Q_EMIT q_ptr->connectionStatusChanged(q_ptr->m_connectionStatus);

    QString server = loginData["server"].toString();
    qDebug() << "Connecting to" << server + ":443";
    socket->connectToHost(server, 443);
}

void WAConnectionPrivate::logout()
{
    socket->disconnectFromHost();
}

void WAConnectionPrivate::sendGetProperties()
{
    QString id = makeId("get_server_properties_");

    ProtocolTreeNode propsNode("props");

    ProtocolTreeNode iqNode("iq");
    AttributeList attrs;
    attrs.insert("id", id);
    attrs.insert("type","get");
    attrs.insert("to", m_domain);
    attrs.insert("xmlns", "w");
    iqNode.setAttributes(attrs);
    iqNode.addChild(propsNode);

    int bytes = sendRequest(iqNode, WAREPLY(connectionServerProperties));
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendPing()
{
    QString id = makeId("ping_");

    ProtocolTreeNode pingNode("ping");

    ProtocolTreeNode iqNode("iq");
    AttributeList attrs;
    attrs.insert("id", id);
    attrs.insert("to", m_domain);
    attrs.insert("type", "get");
    attrs.insert("xmlns", "w:p");
    iqNode.setAttributes(attrs);
    iqNode.addChild(pingNode);

    int bytes = sendRequest(iqNode);
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendGetLastSeen(const QString &jid)
{
    if (jid.contains("-"))
        return;

    QString id = makeId("lastseen_");

    ProtocolTreeNode queryNode("query");

    ProtocolTreeNode iqNode("iq");

    AttributeList attrs;
    attrs.insert("id", id);
    attrs.insert("type", "get");
    attrs.insert("to", jid);
    attrs.insert("xmlns", "jabber:iq:last");
    iqNode.setAttributes(attrs);
    iqNode.addChild(queryNode);

    int bytes = sendRequest(iqNode, WAREPLY(contactLastSeen));
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendGetStatuses(const QStringList &jids)
{
    QString id = makeId("getstatus_");

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
    attrs.insert("id", id);
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
    AttributeList attrs;

    QString id = makeId("get_picture_");

    ProtocolTreeNode pictureNode("picture");
    attrs.insert("type", "image");
    pictureNode.setAttributes(attrs);

    ProtocolTreeNode iqNode("iq");

    attrs.clear();
    attrs.insert("id", id);
    attrs.insert("type", "get");
    attrs.insert("to", jid);
    attrs.insert("xmlns", "w:profile:picture");
    iqNode.setAttributes(attrs);
    iqNode.addChild(pictureNode);

    int bytes = sendRequest(iqNode, WAREPLY(contactPicture));
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendGetPictureIds(const QStringList &jids)
{
    QString id = makeId("get_picture_id_");

    ProtocolTreeNode listNode("list");

    AttributeList attrs;

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

    ProtocolTreeNode iqNode("iq");
    attrs.clear();
    attrs.insert("id", id);
    attrs.insert("to", m_domain);
    attrs.insert("type", "get");
    attrs.insert("xmlns", "w:profile:picture");
    iqNode.setAttributes(attrs);
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

void WAConnectionPrivate::sendNormalText(const QString &jid, const QString &text)
{
    ProtocolTreeNode body = getTextBody(text);
    ProtocolTreeNode message = getMessageNode(jid, "text");
    message.addChild(body);
    int bytes = sendRequest(message);
}

void WAConnectionPrivate::sendBroadcastText(const QString &jid, const QString &text, const QStringList &jids)
{
    ProtocolTreeNode body = getTextBody(text);
    ProtocolTreeNode broadcast = getBroadcastNode(jids);
    ProtocolTreeNode message = getMessageNode(jid, "text");
    message.addChild(broadcast);
    message.addChild(body);
    int bytes = sendRequest(message);
}

void WAConnectionPrivate::sendMessageRead(const QString &jid, const QString &msgId, const QString &participant)
{
    sendMessageReceived(jid, msgId, QString("read"), participant);
}

void WAConnectionPrivate::syncContacts(const QVariantMap &contacts)
{
    syncing = contacts;
    sendSync(syncing.keys(), QStringList());
}

void WAConnectionPrivate::sendSync(const QStringList &syncContacts, const QStringList &deleteJids, int syncType, int index, bool last)
{
    QString id = makeId("sendsync_");

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
    attrs.insert("sid", QString::number((QDateTime::currentDateTimeUtc().toTime_t() + 11644477200) * 10000000));
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
    attrs.insert("id", id);
    attrs.insert("xmlns", "urn:xmpp:whatsapp:sync");
    attrs.insert("type", "get");
    iqNode.setAttributes(attrs);
    iqNode.addChild(syncNode);

    int bytes = sendRequest(iqNode, WAREPLY(syncResponse));
}

void WAConnectionPrivate::sendGetGroups(const QString &type)
{
    QString id = makeId("get_groups_");

    ProtocolTreeNode iqNode("iq");

    AttributeList attrs;
    attrs.insert("id", id);
    attrs.insert("type", "get");
    attrs.insert("to", "g.us");
    attrs.insert("xmlns", "w:g2");
    iqNode.setAttributes(attrs);

    ProtocolTreeNode childNode(type);
    iqNode.addChild(childNode);

    int bytes = sendRequest(iqNode, WAREPLY(groupsResponse));
}

int WAConnectionPrivate::sendRequest(const ProtocolTreeNode &node)
{
    return out->write(node, false);
}

int WAConnectionPrivate::sendRequest(const ProtocolTreeNode &node, const char *member)
{
    m_bindStore[node.getAttributeValue("id")] = member;

    return sendRequest(node);
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

int WAConnectionPrivate::sendFeatures()
{
    ProtocolTreeNode child1("readreceipts");
    ProtocolTreeNode child2("groups_v2");
    ProtocolTreeNode child3("privacy");
    ProtocolTreeNode child4("presence");

    ProtocolTreeNode node("stream:features");
    node.addChild(child1);
    node.addChild(child2);
    node.addChild(child3);
    node.addChild(child4);

    return sendRequest(node);
}

int WAConnectionPrivate::sendAuth()
{
    QByteArray data;
    if (m_nextChallenge.size() > 0)
        data = getAuthBlob(m_nextChallenge);

    AttributeList attrs;
    attrs.insert("passive", "false");
    attrs.insert("mechanism", "WAUTH-2");
    attrs.insert("user", m_username);

    ProtocolTreeNode node("auth", data);
    node.setAttributes(attrs);
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

    qint64 totalSeconds = QDateTime::currentMSecsSinceEpoch() / 1000;
    list.append(QString::number(totalSeconds).toUtf8());
    list.append(m_userAgent);
    if (!m_mcc.isEmpty() && !m_mnc.isEmpty()) {
        list.append(QString(" MccMnc/%1%2").arg(m_mcc).arg(m_mnc));
    }

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
    if (node.getTag() == "success") {
        AttributeList accountData = node.getAttributes();

        if (node.getAttributeValue("status") == "expired") {
            Q_EMIT q_ptr->accountExpired(accountData);

            logout();
            return;
        }

        accountData["nextChallenge"] = QString(node.getData().toBase64());

        Q_EMIT q_ptr->authSuccess(accountData);

        //sendClientConfig();

        //sendPing();

        //lastActivity = QDateTime::currentDateTime().toTime_t();

        q_ptr->m_connectionStatus = WAConnection::LoggedIn;
        Q_EMIT q_ptr->connectionStatusChanged(q_ptr->m_connectionStatus);
    }
    else {
        Q_EMIT q_ptr->authFailed(node.getAttributes());

        logout();
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

QString WAConnectionPrivate::makeId(const QString &prefix)
{
    return prefix + QString::number(++iqid, 16);
}

QString WAConnectionPrivate::messageId()
{
    QWriteLocker locker(&lockSeq);
    QString msgId = QString("%1-%2").arg(QDateTime::currentDateTime().toTime_t()).arg(mseq++);
    return msgId;
}

void WAConnectionPrivate::sendClientConfig()
{
    QString id = makeId("config_");

    AttributeList attrs;
    attrs.insert("id", id);
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
    ProtocolTreeNode receipt("receipt");
    AttributeList attrs;
    attrs.insert("to", jid);
    attrs.insert("id", msdId);
    attrs.insert("t", QString::number(QDateTime::currentMSecsSinceEpoch()));
    if (!participant.isEmpty()) {
        attrs.insert("participant", participant);
    }
    if (!type.isEmpty()) {
        attrs.insert("type", type);
    }
    receipt.setAttributes(attrs);

    int bytes = sendRequest(receipt);
}

void WAConnectionPrivate::sendReceiptAck(const ProtocolTreeNode &node)
{
    QString id = node.getAttributeValue("id");
    QString type = node.getAttributeValue("type");

    AttributeList attrs;
    attrs.insert("class","receipt");
    attrs.insert("type", type.isEmpty() ? "delivery" : type);
    attrs.insert("id", id);
    ProtocolTreeNode ackNode("ack", attrs);

    int bytes = sendRequest(ackNode);
    //counters->increaseCounter(DataCounters::ProtocolBytes, 0, bytes);
}

void WAConnectionPrivate::sendCleanDirty(const QStringList &categories)
{
    QString id = makeId("clean_dirty_");
    ProtocolTreeNode iqNode("iq");
    AttributeList attrs;
    attrs.insert("id", id);
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

void WAConnectionPrivate::parseMessage(const ProtocolTreeNode &node)
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
    }
    else if (type == "media") {
        if (node.getChildren().contains("media")) {
            ProtocolTreeNode mediaNode = node.getChild("media");
            Q_EMIT q_ptr->mediaMessageReceived(jid, id, timestamp, node.getAttributeValue("participant"), node.getAttributes().contains("offline"), mediaNode.getAttributes(), mediaNode.getData());
        }
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

ProtocolTreeNode WAConnectionPrivate::getMessageNode(const QString &jid, const QString &type)
{
    ProtocolTreeNode messageNode("message");
    AttributeList attrs;
    attrs.insert("id", messageId());
    attrs.insert("type", type);
    attrs.insert("to", jid);
    messageNode.setAttributes(attrs);
    return messageNode;
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
            else if (tag == "message")
            {
                parseMessage(node);
                sendMessageReceived(node.getAttributeValue("from"), node.getAttributeValue("id"), QString(), node.getAttributeValue("participant"));
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
                handled = true;
            }
            else if (tag == "receipt")
            {
                Q_EMIT q_ptr->messageReceipt(node.getAttributeValue("from"), node.getAttributeValue("id"), node.getAttributeValue("participant"), node.getAttributeValue("t"), node.getAttributeValue("type"));
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
    qDebug() << "connected";
    socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    q_ptr->m_connectionStatus = WAConnection::Connected;
    Q_EMIT q_ptr->connectionStatusChanged(q_ptr->m_connectionStatus);

    tryLogin();
}

void WAConnectionPrivate::socketDisconnected()
{
    qDebug() << "disconnected";
    QObject::disconnect(socket, 0, 0, 0);
    out->reset();
    in->reset();
    iqid = 0;
    mseq = 0;

    q_ptr->m_connectionStatus = WAConnection::Disconnected;
    Q_EMIT q_ptr->connectionStatusChanged(q_ptr->m_connectionStatus);
}

void WAConnectionPrivate::socketError(QAbstractSocket::SocketError error)
{
    qDebug() << "error:" << error;
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
}

void WAConnectionPrivate::contactsPicrureIds(const ProtocolTreeNode &node)
{
    if (node.getChildren().contains("list")) {
        QVariantMap contacts;
        ProtocolTreeNode list = node.getChild("list");
        ProtocolTreeNodeListIterator i(list.getChildren());
        while (i.hasNext())
        {
            ProtocolTreeNode child = i.next().value();
            if (child.getTag() == "user") {
                QString jid = child.getAttributeValue("jid");
                if (child.getAttributes().contains("id")) {
                    //Q_EMIT q_ptr->contactPictureId(jid, child.getAttributeValue("id"), jid);
                    contacts[jid] = child.getAttributeValue("id");
                }
                else {
                    sendGetPicture(jid);
                    //contacts[jid] == "hidden";
                }
            }
        }
        if (contacts.size() > 0) {
            Q_EMIT q_ptr->contactsPictureIds(contacts);
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
        qDebug() << synced;
        Q_EMIT q_ptr->contactsSynced(synced);
    }
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
        }
    }
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

void WAConnection::sendUnavailable()
{
    d_ptr->sendSetPresence(false);
}

void WAConnection::sendGetGroups(const QString &type)
{
    d_ptr->sendGetGroups(type);
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

void WAConnection::sendNormalText(const QString &jid, const QString &text)
{
    d_ptr->sendNormalText(jid, text);
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
