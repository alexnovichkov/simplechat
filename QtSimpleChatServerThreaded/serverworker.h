#ifndef SERVERWORKER_H
#define SERVERWORKER_H

#include <QObject>
#include <QTcpSocket>
#include <QReadWriteLock>
#include <QUuid>

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
    QString uuid() const;
    void sendJson(const QJsonObject &json);
public slots:
    void disconnectFromClient();
private slots:
    void receiveJson();
signals:
    void jsonReceived(const QJsonObject &jsonDoc);
    void disconnectedFromClient();
    void error();
    void logMessage(MessageType type, const QString &msg);
private:
    QTcpSocket *m_serverSocket;
    QString m_userName;
    mutable QReadWriteLock m_userNameLock;
    QUuid m_uuid;
};

#endif // SERVERWORKER_H
