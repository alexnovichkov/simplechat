#ifndef SERVERWORKER_H
#define SERVERWORKER_H

#include <QObject>
#include <QTcpSocket>
#include <QReadWriteLock>
#include <QUuid>
#include <QSet>

#include "enums.h"

class QJsonObject;


class ServerWorker : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ServerWorker)
public:
    explicit ServerWorker(QObject *parent = nullptr);
    virtual bool setSocketDescriptor(qintptr socketDescriptor);
    QString userName() const;
    void setUserName(const QString &userName);
    QString uid() const;
    void setUid(const QString &uid);
    void sendJson(const QJsonObject &json);

    bool messageProcessed(int messageID) const;
    void addMessage(int messageID);
public slots:
    void disconnectFromClient();
private slots:
    void receiveJson();
signals:
    void jsonReceived(const QJsonObject &jsonDoc);
    void disconnectedFromClient();
    void error(int errorCode);
    void logMessage(MessageType type, const QString &msg);
private:
    QTcpSocket *m_serverSocket;
    QString m_userName;
    QString m_uid;
    QSet<int> m_messages;
    mutable QReadWriteLock m_userNameLock;
    mutable QReadWriteLock m_uidLock;
    mutable QReadWriteLock m_messagesLock;
};

#endif // SERVERWORKER_H
