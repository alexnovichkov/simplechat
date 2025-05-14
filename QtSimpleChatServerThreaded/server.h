#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include "enums.h"

class ChatServer;

class Server : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Server)
public:
    explicit Server(QObject *parent = nullptr);
    void toggleStartServer();
private:
    ChatServer *m_chatServer;
private slots:
    void logMessage(MessageType type, const QString &msg);
};

#endif // SERVER_H
