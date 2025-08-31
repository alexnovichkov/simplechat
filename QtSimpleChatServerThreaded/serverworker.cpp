#include "serverworker.h"
#include <QDataStream>
#include <QCborValue>


ServerWorker::ServerWorker(QObject *parent)
    : QObject(parent)
    , m_socket(this), m_reader(&m_socket), m_writer(&m_socket)
{

    connect(&m_socket, &QTcpSocket::connected, this, [this](){
        if (!m_socket.isOpen()) {
            // qDebug() << "Error: socket is not open";
        }
        if (!m_writer.device()) {
            // qDebug() << "No writer device set";
            m_writer.setDevice(&m_socket);
        }
        if (!m_reader.device()) {
            // qDebug() << "No reader device set";
            m_reader.setDevice(&m_socket);
        }
    });

    connect(&m_socket, &QTcpSocket::readyRead, this, &ServerWorker::receiveData);
    connect(&m_socket, &QTcpSocket::disconnected, this, &ServerWorker::disconnectedFromClient);
#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
    connect(m_serverSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &ServerWorker::error);
#else
    connect(&m_socket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error){
        emit this->error(static_cast<int>(error));
    });
#endif
}

ServerWorker::~ServerWorker()
{
    if (m_writeOpened && m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_writer.endArray();
        m_socket.waitForBytesWritten(2000);
    }
}


bool ServerWorker::setSocketDescriptor(qintptr socketDescriptor)
{
    return m_socket.setSocketDescriptor(socketDescriptor);
}

void ServerWorker::sendData(const QMap<int, QVariant> &data)
{
    // qDebug() << "Sending"<<data;
    if (!m_writeOpened) {
        // qDebug() << "starting the main array";
        m_writer.startArray();
        m_writeOpened = true;
    }
    m_writer.startMap(data.size());
    for (auto i = data.cbegin(); i != data.cend(); ++i)
    {
        m_writer.append(i.key());
        switch (i.value().type()) {
            case QVariant::Bool: m_writer.append(i.value().toBool()); break;
            case QVariant::Int: m_writer.append(i.value().toInt()); break;
            case QVariant::UInt: m_writer.append(i.value().toUInt()); break;
            case QVariant::LongLong: m_writer.append(i.value().toLongLong()); break;
            case QVariant::ULongLong: m_writer.append(i.value().toULongLong()); break;
            case QVariant::Double: m_writer.append(i.value().toDouble()); break;
            case QVariant::Char: m_writer.append(i.value().toString()); break;
            case QVariant::Map: { // QMap<QString, QString>
                auto map = i.value().toMap();
                m_writer.startMap(map.size());
                for (auto j = map.cbegin(); j != map.cend(); ++j) {
                    m_writer.append(j.key());
                    m_writer.append(j.value().toString());
                }
                m_writer.endMap();
                break;
            }
            case QVariant::List: { // QList<QVariant>
                auto list = i.value().toList();
                m_writer.startArray(list.size());
                for (auto j = 0; j < list.size(); ++j) {
                    m_writer.append(list.at(j).toString());
                }
                m_writer.endArray();
                break;
            }
            case QVariant::String: m_writer.append(i.value().toString()); break;
            case QVariant::StringList: {
                auto list = i.value().toStringList();
                m_writer.startArray(list.size());
                for (auto j = 0; j < list.size(); ++j) {
                    m_writer.append(list.at(j));
                }
                m_writer.endArray();
                break;
            }
            case QVariant::ByteArray: m_writer.append(i.value().toByteArray()); break;
            default: {
                emit logMessage(MessageType::Critical, QString("Unknown type of data: %1").arg(i.key()));
                break;
            }
        }
    }
    m_writer.endMap();
}

// bool ServerWorker::messageProcessed(int messageID) const
// {
//     m_messagesLock.lockForRead();
//     const bool processed = m_messages.contains(messageID);
//     m_messagesLock.unlock();
//     return processed;
// }

// void ServerWorker::addMessage(int messageID)
// {
//     m_messagesLock.lockForWrite();
//     m_messages.insert(messageID);
//     m_messagesLock.unlock();
// }

void ServerWorker::disconnectFromClient()
{
    m_socket.disconnectFromHost();
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

void ServerWorker::receiveData()
{
    m_reader.reparse();
    // qDebug() << "last reader error:"<<m_reader.lastError().toString();

    // Протокол:
    // [
    // {Type, Val}
    // ...
    // ]

    while (m_reader.lastError() == QCborError::NoError) {
        if (!m_started) {
            // qDebug() << "not started yet";
            if (!m_reader.isArray()) {
                // qDebug() << "Error: must be an array";
                break; // protocol error
            }
            // qDebug() << "entering the main array";
            m_reader.enterContainer();
            m_started = true;
        }
        else if (m_reader.containerDepth() == 1) {
            // qDebug() << "we are at depth 1";
            if (!m_reader.hasNext()) {
                // qDebug() << "nothing to read, disconnecting...";
                m_reader.leaveContainer();
                // disconnectFromHost();
                return;
            }

            if (!m_reader.isMap() || !m_reader.isLengthKnown()) {
                // qDebug() << "Error: must be a map";
                break; // protocol error
            }
            // qDebug() << "we are at a map. Receiving message";
            m_leftToRead = m_reader.length();
            // qDebug() << "message size is"<<m_leftToRead;
            m_reader.enterContainer();
            m_receivedData.clear();
        }
        else if (m_lastMessageType == Unknown) {
            // qDebug() << "reading message type";
            if (!m_reader.isInteger()) {
                // qDebug() << "Error: message type must be an integer";
                break; // protocol error
            }
            m_lastMessageType = Type(m_reader.toInteger());
            // qDebug() << "message type"<<m_lastMessageType;
            m_reader.next();
        }
        else {
            // qDebug() << "reading payload";
            switch (m_reader.type()) {
                case QCborStreamReader::UnsignedInteger:
                case QCborStreamReader::NegativeInteger: {
                    m_receivedData.insert(m_lastMessageType, m_reader.toInteger());
                    m_reader.next();
                    break;
                }
                case QCborStreamReader::Float:
                case QCborStreamReader::Double: {
                    m_receivedData.insert(m_lastMessageType, m_reader.toDouble());
                    m_reader.next();
                    break;
                }
                case QCborStreamReader::ByteString: {
                    m_receivedData.insert(m_lastMessageType, handleByteArray());
                    break;
                }
                case QCborStreamReader::TextString: {
                    m_receivedData.insert(m_lastMessageType, handleString());
                    break;
                }
                case QCborStreamReader::Array: {
                    m_receivedData.insert(m_lastMessageType, handleArray());
                    break;
                }
                case QCborStreamReader::Map: {
                    m_receivedData.insert(m_lastMessageType, handleMap());
                    break;
                }
                case QCborStreamReader::SimpleType: { // treat as bool
                    m_receivedData.insert(m_lastMessageType, m_reader.toBool());
                    m_reader.next();
                    break;
                }
                default: {
                    // qDebug() << "Error: unknown message payload";
                    m_reader.next(); // skip unknown value
                    break;
                }
            }
            m_leftToRead--;
            // qDebug() << "read so far:"<<m_receivedData;
            // qDebug() << "left to read:"<<m_leftToRead;

            if (m_leftToRead == 0) {
                m_reader.leaveContainer();
                // qDebug() << "The total message data:"<<m_receivedData;
                emit dataReceived(m_receivedData);
            }
            m_lastMessageType = Unknown;
        }
    }

    if (m_reader.lastError() != QCborError::NoError) {
        emit logMessage(MessageType::Warning, QLatin1String("Invalid message: ") + m_reader.lastError().toString());
    }
}

QByteArray ServerWorker::handleByteArray()
{
    QByteArray result;
    auto r = m_reader.readByteArray();
    while (r.status == QCborStreamReader::Ok) {
        result += r.data;
        r = m_reader.readByteArray();
    }

    if (r.status == QCborStreamReader::Error)
        result.clear();

    return result;
}

QString ServerWorker::handleString()
{
    QString result;
    auto r = m_reader.readString();
    while (r.status == QCborStreamReader::Ok) {
        result += r.data;
        r = m_reader.readString();
    }

    if (r.status == QCborStreamReader::Error)
        result.clear();

    return result;
}

QVariantList ServerWorker::handleArray()
{
    QVariantList result;

    if (m_reader.isLengthKnown())
        result.reserve(m_reader.length());

    m_reader.enterContainer();
    while (m_reader.lastError() == QCborError::NoError && m_reader.hasNext())
        result.append(handleString());

    if (m_reader.lastError() == QCborError::NoError)
        m_reader.leaveContainer();

    return result;
}

QVariantMap ServerWorker::handleMap()
{
    QVariantMap result;

    m_reader.enterContainer();
    while (m_reader.lastError() == QCborError::NoError && m_reader.hasNext()) {
        QString key = handleString();
        result.insert(key, handleString());
    }

    if (m_reader.lastError() == QCborError::NoError)
        m_reader.leaveContainer();

    return result;
}


