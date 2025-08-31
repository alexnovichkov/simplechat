#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QTcpServer>
#include <QVector>
#include <QTimer>
#include <QReadWriteLock>

class QThread;
class ServerWorker;

#include "enums.h"

class ChatServer : public QTcpServer
{
    Q_OBJECT
    Q_DISABLE_COPY(ChatServer)
public:
    explicit ChatServer(QObject *parent = nullptr);
    ~ChatServer();
protected:
    void incomingConnection(qintptr socketDescriptor) override;
private:
    const int m_idealThreadCount;
    QVector<QThread *> m_availableThreads;
    QVector<int> m_threadsLoad;
    QVector<ServerWorker *> m_clients;
    QTimer timer;
    mutable QReadWriteLock m_clientsLock;
private slots:
    void send(const QMap<int, QVariant> &message, const QString &receiverUid);
    void broadcast(const QMap<int, QVariant> &message, ServerWorker *exclude);
    void dataReceived(const QMap<int, QVariant> &data);
    void userDisconnected(ServerWorker *sender, int threadIdx);
    void userError(ServerWorker *sender, int error);
public slots:
    void stopServer();
private:
    void dataFromLoggedOut(ServerWorker *sender, const QMap<int, QVariant> &data);
    void dataFromLoggedIn(ServerWorker *sender, const QMap<int, QVariant> &data);
    void sendData(ServerWorker *destination, const QMap<int, QVariant> &data);
    QVariantList loggedInUsers(ServerWorker *exclude) const;
signals:
    void logMessage(MessageType type, const QString &msg);
    void stopAllClients();
};

#endif // CHATSERVER_H
