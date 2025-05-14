#include "server.h"

#include "chatserver.h"

#include <QDateTime>

Server::Server(QObject *parent)
    : QObject(parent)
    , m_chatServer(new ChatServer(this))
{
    connect(m_chatServer, &ChatServer::logMessage, this, &Server::logMessage);
}

void Server::toggleStartServer()
{
    if (m_chatServer->isListening()) {
        m_chatServer->stopServer();
        logMessage(MessageType::Info, QStringLiteral("Server Stopped"));
    } else {
        if (!m_chatServer->listen(QHostAddress::Any, SERVER_PORT)) {
            logMessage(MessageType::Critical, QStringLiteral("Unable to start the server"));
            return;
        }
        logMessage(MessageType::Info, QStringLiteral("Server Started"));
    }
}

void Server::logMessage(MessageType type, const QString &msg)
{
    auto dt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    switch(type) {
        case MessageType::Info:
            qDebug()    << dt << QStringLiteral("INFO:   ") << msg;
            break;
        case MessageType::Warning:
            qWarning()  << dt << QStringLiteral("WARNING:") << msg;
            break;
        case MessageType::Critical:
            qCritical() << dt << QStringLiteral("ERROR:  ") << msg;
            break;
    }
}
