#include "chatserver.h"
#include "serverworker.h"
#include <QThread>
#include <functional>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QTimer>



ChatServer::ChatServer(QObject *parent)
    : QTcpServer(parent)
    , m_idealThreadCount(qMax(QThread::idealThreadCount(), 1))
{
    m_availableThreads.reserve(m_idealThreadCount);
    m_threadsLoad.reserve(m_idealThreadCount);
}

ChatServer::~ChatServer()
{
    for (QThread *singleThread : m_availableThreads) {
        singleThread->quit();
        singleThread->wait();
    }
}
void ChatServer::incomingConnection(qintptr socketDescriptor)
{
    emit logMessage(MessageType::Info,
                    QStringLiteral("Incoming connection from %1...").arg(socketDescriptor));
    ServerWorker *worker = new ServerWorker;
    if (!worker->setSocketDescriptor(socketDescriptor)) {
        emit logMessage(MessageType::Critical,
                        QStringLiteral("Error in setting the connection "
                                       "with socket descriptor %1.").arg(socketDescriptor));
        worker->deleteLater();
        return;
    }
    int threadIdx = m_availableThreads.size();
    if (threadIdx < m_idealThreadCount) { //we can add a new thread
        m_availableThreads.append(new QThread(this));
        m_threadsLoad.append(1);
        m_availableThreads.last()->start();
    } else {
        // find the thread with the least amount of clients and use it
        threadIdx = std::distance(m_threadsLoad.cbegin(), std::min_element(m_threadsLoad.cbegin(), m_threadsLoad.cend()));
        ++m_threadsLoad[threadIdx];
    }
    worker->moveToThread(m_availableThreads.at(threadIdx));
    connect(m_availableThreads.at(threadIdx), &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &ServerWorker::disconnectedFromClient, this,
            std::bind(&ChatServer::userDisconnected, this, worker, threadIdx));
    connect(worker, &ServerWorker::error, this, std::bind(&ChatServer::userError, this, worker, std::placeholders::_1));
    connect(worker, &ServerWorker::jsonReceived, this,
            std::bind(&ChatServer::jsonReceived, this, worker, std::placeholders::_1));
    connect(worker, &ServerWorker::logMessage, this, &ChatServer::logMessage);
    connect(this, &ChatServer::stopAllClients, worker, &ServerWorker::disconnectFromClient);
    m_clients.append(worker);
    emit logMessage(MessageType::Info, QStringLiteral("New client connected from %1").arg(socketDescriptor));
}

void ChatServer::send(const QJsonObject &message, const QString &receiverUid)
{
    for (ServerWorker *worker : qAsConst(m_clients)) {
        Q_ASSERT(worker);
        if (worker->uid() == receiverUid) {
            sendJson(worker, message);
            break; // we assume that there can only be one worker with the receiverUUid
        }
    }
}

void ChatServer::broadcast(const QJsonObject &message, ServerWorker *exclude)
{
    for (ServerWorker *worker : qAsConst(m_clients)) {
        Q_ASSERT(worker);
        if (worker != exclude)
            sendJson(worker, message);
    }
}

void ChatServer::sendJson(ServerWorker *destination, const QJsonObject &message)
{
    Q_ASSERT(destination);
    emit logMessage(MessageType::Info,
                    QStringLiteral("Sending \"%1\" to %2")
                        .arg(message[QStringLiteral("type")].toString())
                        .arg(destination->uid()));
    QTimer::singleShot(0, destination, std::bind(&ServerWorker::sendJson, destination, message));
}

QJsonArray ChatServer::loggedInUsers(ServerWorker *exclude) const
{
    QJsonArray users;
    for (ServerWorker *worker : m_clients) {
        if (worker == exclude) continue;
        Q_ASSERT(worker);
        if (auto name = worker->userName(); !name.isEmpty()) {
            // QJsonObject user;
            // user[QStringLiteral("userName")] = name;
            // user[QStringLiteral("uid")] = worker->uid();
            users.append(name+"\n"+worker->uid());
        }
    }
    return users;
}

void ChatServer::jsonReceived(ServerWorker *sender, const QJsonObject &json)
{
    Q_ASSERT(sender);
    emit logMessage(MessageType::Info,
                    QLatin1String("JSON received \"%1\" from %2")
                        .arg(json[QStringLiteral("type")].toString())
                        .arg(sender->uid()));
    if (sender->userName().isEmpty())
        // a new user is trying to log in
        jsonFromLoggedOut(sender, json);
    else
        // a message from the known user
        jsonFromLoggedIn(sender, json);
}

void ChatServer::userDisconnected(ServerWorker *sender, int threadIdx)
{
    --m_threadsLoad[threadIdx];
    m_clients.removeAll(sender);
    const QString userName = sender->userName();
    if (!userName.isEmpty()) {
        QJsonObject disconnectedMessage;
        disconnectedMessage[QStringLiteral("type")] = QStringLiteral("userdisconnected");
        disconnectedMessage[QStringLiteral("username")] = userName;
        disconnectedMessage[QStringLiteral("uid")] = sender->uid();
        broadcast(disconnectedMessage, nullptr);
        emit logMessage(MessageType::Info, sender->uid() + QLatin1String(" disconnected"));
    }
    sender->deleteLater();
}

void ChatServer::userError(ServerWorker *sender, int error)
{
    Q_UNUSED(sender)
    emit logMessage(MessageType::Critical, QLatin1String("Error from %1: %2")
                                               .arg(sender->uid())
                                               .arg(error));
}

void ChatServer::stopServer()
{
    emit stopAllClients();
    close();
}

void ChatServer::jsonFromLoggedOut(ServerWorker *sender, const QJsonObject &docObj)
{
    Q_ASSERT(sender);
    const auto typeVal = docObj.value(QLatin1String("type")).toString();
    if (typeVal.toLower() != QStringLiteral("login")) {
        emit logMessage(MessageType::Warning,
                        QStringLiteral("Wrong message \"%1\" from an unauthorized client.")
                            .arg(typeVal));
        return;
    }

    const auto newUserName = docObj.value(QLatin1String("username")).toString().simplified();
    if (newUserName.isEmpty()) {
        emit logMessage(MessageType::Warning,
                        QStringLiteral("New client \"%1\" has empty username.")
                            .arg(sender->uid()));
        return;
    }
    const auto userUid = docObj.value(QLatin1String("uid")).toString();
    if (userUid.isEmpty()) {
        emit logMessage(MessageType::Warning,
                        QStringLiteral("New client \"%1\" has empty uid.")
                            .arg(newUserName));
        return;
    }

    // search for duplicate username
    for (ServerWorker *worker : qAsConst(m_clients)) {
        if (worker != sender) {
            if (worker->userName() == newUserName) {
                QJsonObject message;
                message[QStringLiteral("type")] = QStringLiteral("login");
                message[QStringLiteral("success")] = false;
                message[QStringLiteral("reason")] = QStringLiteral("duplicate username");
                sendJson(sender, message);
                emit logMessage(MessageType::Critical,
                                QStringLiteral("Clients %1 and %2 have duplicate username \"%3\".")
                                    .arg(worker->uid())
                                    .arg(sender->uid())
                                    .arg(newUserName));
                return;
            }
        }
    }

    sender->setUserName(newUserName);
    sender->setUid(userUid);

    // send back the login sucess
    QJsonObject successMessage;
    successMessage[QStringLiteral("type")] = QStringLiteral("login");
    successMessage[QStringLiteral("success")] = true;
    const auto users = loggedInUsers(sender);
    if (!users.isEmpty())
        successMessage[QStringLiteral("users")] = users;
    sendJson(sender, successMessage);

    // broadcast the new user
    QJsonObject newUserMessage;
    newUserMessage[QStringLiteral("type")] = QStringLiteral("newuser");
    newUserMessage[QStringLiteral("username")] = newUserName;
    newUserMessage[QStringLiteral("uid")] = userUid;
    broadcast(newUserMessage, sender);
    emit logMessage(MessageType::Info,
                    QStringLiteral("Login successful: %1 as \"%2\"")
                        .arg(userUid)
                        .arg(newUserName));
}

void ChatServer::jsonFromLoggedIn(ServerWorker *sender, const QJsonObject &docObj)
{
    Q_ASSERT(sender);

    // broadcast the message with additional info on the sender
    QJsonObject message = docObj;
    message[QStringLiteral("sender")] = sender->userName();
    message[QLatin1String("senderUid")] = sender->uid();
    auto receiver = docObj.value(QStringLiteral("receiver")).toString();
    if (receiver == QLatin1String("all") || receiver.isEmpty())
        broadcast(message, sender); // broadcast the message to all users in the chat
    else
        send(message, receiver); // send the message to a receiver only
}


