#include "chatserver.h"
#include "serverworker.h"
#include <QThread>
#include <functional>
#include <QTimer>

ChatServer::ChatServer(QObject *parent)
    : QTcpServer(parent)
    , m_idealThreadCount(qMax(QThread::idealThreadCount(), 1))
{
    qRegisterMetaType<QMap<int, QVariant>>();
    m_availableThreads.reserve(m_idealThreadCount);
    m_threadsLoad.reserve(m_idealThreadCount);
}

ChatServer::~ChatServer()
{
    for (QThread *singleThread : qAsConst(m_availableThreads)) {
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
    connect(worker, &ServerWorker::dataReceived, this, &ChatServer::dataReceived);
    connect(worker, &ServerWorker::logMessage, this, &ChatServer::logMessage);
    connect(this, &ChatServer::stopAllClients, worker, &ServerWorker::disconnectFromClient);

    m_clientsLock.lockForWrite();
    m_clients.append(worker);
    m_clientsLock.unlock();
    emit logMessage(MessageType::Info, QStringLiteral("New client connected from %1").arg(socketDescriptor));
}

void ChatServer::send(const QMap<int, QVariant> &message, const QString &receiverUid)
{
    m_clientsLock.lockForRead();
    const auto clients = m_clients;
    m_clientsLock.unlock();
    for (ServerWorker *worker : clients) {
        Q_ASSERT(worker);
        if (worker->uid() == receiverUid) {
            sendData(worker, message);
            break; // we assume that there can only be one worker with the receiverUid
        }
    }
}

void ChatServer::broadcast(const QMap<int, QVariant> &message, ServerWorker *exclude)
{
    m_clientsLock.lockForRead();
    const auto clients = m_clients;
    m_clientsLock.unlock();
    for (ServerWorker *worker : clients) {
        Q_ASSERT(worker);
        if (worker != exclude)
            sendData(worker, message);
    }
}

void ChatServer::sendData(ServerWorker *destination, const QMap<int, QVariant> &message)
{
    Q_ASSERT(destination);
    emit logMessage(MessageType::Info,
                    QStringLiteral("Sending \"%1\" to %2")
                        .arg(message[DataType].toString())
                        .arg(destination->uid()));
    QTimer::singleShot(0, destination, std::bind(&ServerWorker::sendData, destination, message));
}

QVariantList ChatServer::loggedInUsers(ServerWorker *exclude) const
{
    QVariantList users;
    m_clientsLock.lockForRead();
    const auto clients = m_clients;
    m_clientsLock.unlock();
    for (ServerWorker *worker : clients) {
        if (worker == exclude) continue;
        Q_ASSERT(worker);
        if (auto name = worker->userName(); !name.isEmpty())
            users.append(QString("%1\n%2\n%3").arg(name).arg(worker->uid()).arg(worker->status()));
    }
    return users;
}

void ChatServer::dataReceived(const QMap<int, QVariant> &data)
{
    ServerWorker *sender = dynamic_cast<ServerWorker*>(this->sender());
    Q_ASSERT(sender);
    emit logMessage(MessageType::Info,
                    QLatin1String("Data received from %1").arg(sender->uid()));

    if (sender->userName().isEmpty())
        // a new user is trying to log in
        dataFromLoggedOut(sender, data);
    else
        // a message from the known user
        dataFromLoggedIn(sender, data);
}

void ChatServer::dataFromLoggedOut(ServerWorker *sender, const QMap<int, QVariant> &data)
{
    Q_ASSERT(sender);
    const auto type = data.value(DataType).toString();
    if (type.toLower() != QStringLiteral("login")) {
        emit logMessage(MessageType::Warning,
                        QStringLiteral("Wrong message \"%1\" from an unauthorized client.")
                            .arg(type));
        return;
    }

    const auto userName = data.value(UserName).toString().simplified();
    if (userName.isEmpty()) {
        emit logMessage(MessageType::Warning,
                        QStringLiteral("New client \"%1\" has empty username.")
                            .arg(sender->uid()));
        return;
    }
    const auto userUid = data.value(UserUid).toString();
    if (userUid.isEmpty()) {
        emit logMessage(MessageType::Warning,
                        QStringLiteral("New client \"%1\" has empty uid.")
                            .arg(userName));
        return;
    }

    // search for duplicate username
    m_clientsLock.lockForRead();
    const auto clients = m_clients;
    m_clientsLock.unlock();
    for (ServerWorker *worker : clients) {
        if (worker != sender) {
            if (worker->userName() == userName) {
                QMap<int, QVariant> message;
                message[DataType] = QStringLiteral("login");
                message[Success] = false;
                message[Reason] = QStringLiteral("Username is already in use");
                sendData(sender, message);
                emit logMessage(MessageType::Critical,
                                QStringLiteral("Clients %1 and %2 have duplicate username \"%3\".")
                                    .arg(worker->uid())
                                    .arg(sender->uid())
                                    .arg(userName));
                return;
            }
        }
    }

    sender->setUserName(userName);
    sender->setUid(userUid);

    // send back the login success
    QMap<int, QVariant> successMessage;
    successMessage[DataType] = QStringLiteral("login");
    successMessage[Success] = true;
    const auto users = loggedInUsers(sender);
    if (!users.isEmpty())
        successMessage[Users] = users;
    sendData(sender, successMessage);

    // broadcast the new user
    QMap<int, QVariant> newUserMessage;
    newUserMessage[DataType] = QStringLiteral("newuser");
    newUserMessage[UserName] = userName;
    newUserMessage[UserUid] = userUid;
    broadcast(newUserMessage, sender);
    emit logMessage(MessageType::Info,
                    QStringLiteral("Login successful: %1 as \"%2\"")
                        .arg(userUid)
                        .arg(userName));
}

void ChatServer::dataFromLoggedIn(ServerWorker *sender, const QMap<int, QVariant> &data)
{
    Q_ASSERT(sender);

    QMap<int, QVariant> message = data;
    message[SenderName] = sender->userName();
    message[SenderUid] = sender->uid();
    auto receiverUid = data.value(ReceiverUid).toString();
    if (receiverUid == QLatin1String("all") || receiverUid.isEmpty())
        broadcast(message, sender); // broadcast the message to all users in the chat
    else
        send(message, receiverUid); // send the message to a receiver only
}

void ChatServer::userDisconnected(ServerWorker *sender, int threadIdx)
{
    --m_threadsLoad[threadIdx];
    m_clientsLock.lockForWrite();
    m_clients.removeAll(sender);
    m_clientsLock.unlock();
    const QString userName = sender->userName();
    if (!userName.isEmpty()) {
        QMap<int, QVariant> message;
        message[DataType] = QStringLiteral("userdisconnected");
        message[UserName] = userName;
        message[UserUid] = sender->uid();
        broadcast(message, nullptr);
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
