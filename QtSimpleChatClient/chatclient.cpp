#include "chatclient.h"
#include <QTcpSocket>
#include <QDataStream>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

ChatClient::ChatClient(QObject *parent)
    : QObject(parent)
    , m_clientSocket(new QTcpSocket(this))
    , m_loggedIn(false)
{
    // Forward the connected and disconnected signals
    connect(m_clientSocket, &QTcpSocket::connected, this, &ChatClient::connected);
    connect(m_clientSocket, &QTcpSocket::disconnected, this, &ChatClient::disconnected);
    // connect readyRead() to the slot that will take care of reading the data in
    connect(m_clientSocket, &QTcpSocket::readyRead, this, &ChatClient::onReadyRead);
    // Forward the error signal, QOverload is necessary as error() is overloaded, see the Qt docs 
#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
    connect(m_clientSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &ChatClient::error);
#else
    connect(m_clientSocket, &QAbstractSocket::errorOccurred, this, &ChatClient::error);
#endif
    // Reset the m_loggedIn variable when we disconnec. Since the operation is trivial we use a lambda instead of creating another slot
    connect(m_clientSocket, &QTcpSocket::disconnected, this, [this]()->void{m_loggedIn = false;});
}

void ChatClient::login(const QString &userName)
{
    if (m_clientSocket->state() == QAbstractSocket::ConnectedState) { // if the client is connected
        // create a QDataStream operating on the socket
        QDataStream clientStream(m_clientSocket);
        // set the version so that programs compiled with different versions of Qt can agree on how to serialise
        clientStream.setVersion(QDataStream::Qt_5_7);
        // Create the JSON we want to send
        QJsonObject message;
        message[QStringLiteral("type")] = QStringLiteral("login");
        message[QStringLiteral("username")] = userName;
        // send the JSON using QDataStream
        clientStream << QJsonDocument(message).toJson(QJsonDocument::Compact);
    }
}

void ChatClient::sendMessage(const QString &text)
{
    if (text.isEmpty())
        return; // We don't send empty messages
    // create a QDataStream operating on the socket
    QDataStream clientStream(m_clientSocket);
    // set the version so that programs compiled with different versions of Qt can agree on how to serialise
    clientStream.setVersion(QDataStream::Qt_5_7);
    // Create the JSON we want to send
    QJsonObject message;
    message[QStringLiteral("type")] = QStringLiteral("message");
    message[QStringLiteral("text")] = text;
    // send the JSON using QDataStream
    clientStream << QJsonDocument(message).toJson();
}

void ChatClient::disconnectFromHost()
{
    m_clientSocket->disconnectFromHost();
}

void ChatClient::jsonReceived(const QJsonObject &docObj)
{
    // actions depend on the type of message
    const auto typeVal = docObj.value(QLatin1String("type")).toString();
    if (typeVal == QLatin1String("login") && !m_loggedIn) { //It's a login message
        // the success field will contain the result of our attempt to login
        const QJsonValue resultVal = docObj.value(QLatin1String("success"));
        if (resultVal.isNull() || !resultVal.isBool())
            return; // the message had no success field so we ignore
        const bool loginSuccess = resultVal.toBool();
        if (loginSuccess) {
            // we logged in succesfully and we notify it via the loggedIn signal
            emit loggedIn();
        }
        else {
            // the login attempt failed, we extract the reason of the failure from the JSON
            // and notify it via the loginError signal
            emit loginError(docObj.value(QLatin1String("reason")).toString());
        }
    } else if (typeVal == QLatin1String("message")) { //It's a chat message
        // we extract the text field containing the chat text
        const auto textVal = docObj.value(QLatin1String("text")).toString();
        // we extract the sender field containing the username of the sender
        const auto senderVal = docObj.value(QLatin1String("sender")).toString();
        if (!textVal.isEmpty() && !senderVal.isEmpty())
            // we notify a new message was received via the messageReceived signal
            emit messageReceived(senderVal, textVal);
    } else if (typeVal == QLatin1String("newuser")) { // A user joined the chat
        // we extract the username of the new user
        const auto usernameVal = docObj.value(QLatin1String("username")).toString();
        if (!usernameVal.isEmpty())
            // we notify of the new user via the userJoined signal
            emit userJoined(usernameVal);
    } else if (typeVal == QLatin1String("userdisconnected")) { // A user left the chat
         // we extract the username of the new user
        const auto usernameVal = docObj.value(QLatin1String("username")).toString();
        if (!usernameVal.isEmpty())
            // we notify of the user disconnection the userLeft signal
            emit userLeft(usernameVal);
    }
    else {
        // a message with no type was received so we just ignore it
    }
}

void ChatClient::connectToServer(const QHostAddress &address, quint16 port)
{
    m_clientSocket->connectToHost(address, port);
}

void ChatClient::onReadyRead()
{
    // prepare a container to hold the UTF-8 encoded JSON we receive from the socket
    QByteArray jsonData;
    // create a QDataStream operating on the socket
    QDataStream socketStream(m_clientSocket);
    // set the version so that programs compiled with different versions of Qt can agree on how to serialise
    socketStream.setVersion(QDataStream::Qt_5_7);
    // start an infinite loop
    for (;;) {
        // we start a transaction so we can revert to the previous state in case we try to read more data than is available on the socket
        socketStream.startTransaction();
        // we try to read the JSON data
        socketStream >> jsonData;
        if (socketStream.commitTransaction()) {
            // we successfully read some data
            // we now need to make sure it's in fact a valid JSON
            QJsonParseError parseError;
            // we try to create a json document with the data we received
            const QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);
            if (parseError.error == QJsonParseError::NoError) {
                // if the data was indeed valid JSON
                if (jsonDoc.isObject()) // and is a JSON object
                    jsonReceived(jsonDoc.object()); // parse the JSON
            }
            // loop and try to read more JSONs if they are available
        } else {
            // the read failed, the socket goes automatically back to the state it was in before the transaction started
            // we just exit the loop and wait for more data to become available
            break;
        }
    }
}
