#include "serverworker.h"
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
ServerWorker::ServerWorker(QObject *parent)
    : QObject(parent)
    , m_serverSocket(new QTcpSocket(this))
{
    connect(m_serverSocket, &QTcpSocket::readyRead, this, &ServerWorker::receiveJson);
    connect(m_serverSocket, &QTcpSocket::disconnected, this, &ServerWorker::disconnectedFromClient);
#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
    connect(m_serverSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &ServerWorker::error);
#else
    connect(m_serverSocket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error){
        emit this->error(static_cast<int>(error));
    });
#endif
}


bool ServerWorker::setSocketDescriptor(qintptr socketDescriptor)
{
    return m_serverSocket->setSocketDescriptor(socketDescriptor);
}

void ServerWorker::sendJson(const QJsonObject &json)
{
    const QByteArray jsonData = QJsonDocument(json).toJson();
    QDataStream socketStream(m_serverSocket);
    socketStream.setVersion(QDataStream::Qt_5_7);
    socketStream << jsonData;
}

bool ServerWorker::messageProcessed(int messageID) const
{
    m_messagesLock.lockForRead();
    const bool processed = m_messages.contains(messageID);
    m_messagesLock.unlock();
    return processed;
}

void ServerWorker::addMessage(int messageID)
{
    m_messagesLock.lockForWrite();
    m_messages.insert(messageID);
    m_messagesLock.unlock();
}

void ServerWorker::disconnectFromClient()
{
    m_serverSocket->disconnectFromHost();
}

QString ServerWorker::userName() const
{
    m_userNameLock.lockForRead();
    const QString result = m_userName;
    m_userNameLock.unlock();
    return result;
}

void ServerWorker::setUserName(const QString &userName)
{
    m_userNameLock.lockForWrite();
    m_userName = userName;
    m_userNameLock.unlock();
}

QString ServerWorker::uid() const
{
    m_uidLock.lockForRead();
    const QString result = m_uid;
    m_uidLock.unlock();
    return result;
}

void ServerWorker::setUid(
    const QString &uid)
{
    m_uidLock.lockForWrite();
    m_uid = uid;
    m_uidLock.unlock();
}

void ServerWorker::receiveJson()
{
    QByteArray jsonData;
    QDataStream socketStream(m_serverSocket);
    socketStream.setVersion(QDataStream::Qt_5_7);
    for (;;) {
        socketStream.startTransaction();
        socketStream >> jsonData;
        if (socketStream.commitTransaction()) {
            QJsonParseError parseError;
            const QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);
            if (parseError.error == QJsonParseError::NoError) {
                if (jsonDoc.isObject())
                    emit jsonReceived(jsonDoc.object());
                else
                    emit logMessage(MessageType::Warning, QLatin1String("Invalid message: ") + QString::fromUtf8(jsonData));
            } else {
                emit logMessage(MessageType::Warning, QLatin1String("Invalid message: ") + QString::fromUtf8(jsonData));
            }
        } else {
            break;
        }
    }
}


