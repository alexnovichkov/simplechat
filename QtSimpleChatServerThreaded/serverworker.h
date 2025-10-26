#ifndef SERVERWORKER_H
#define SERVERWORKER_H

#include <QObject>
#include <QTcpSocket>
#include <QReadWriteLock>
#include <QUuid>
#include <QSet>

#include "enums.h"

#include <QCborStreamReader>
#include <QCborStreamWriter>


class ServerWorker : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ServerWorker)
public:
    explicit ServerWorker(QObject *parent = nullptr);
    ~ServerWorker();
    virtual bool setSocketDescriptor(qintptr socketDescriptor);
    QString userName() const;
    void setUserName(const QString &userName);
    QString uid() const;
    void setUid(const QString &uid);
    int status() const;
    void sendData(const QMap<int, QVariant> &data);

    // bool messageProcessed(int messageID) const;
    // void addMessage(int messageID);
public slots:
    void disconnectFromClient();
private slots:
    // void receiveJson();
    void receiveData();
signals:
    void dataReceived(const QMap<int, QVariant> &data);
    void disconnectedFromClient();
    void error(int errorCode);
    void logMessage(MessageType type, const QString &msg);
private:
    QByteArray handleByteArray();
    QString handleString();
    QVariantList handleArray();
    QVariantMap handleMap();

    QTcpSocket m_socket;
    QCborStreamReader m_reader;
    QCborStreamWriter m_writer;

    QString m_userName;
    QString m_uid;
    int m_status{0}; //offline
    mutable QReadWriteLock m_userNameLock;
    mutable QReadWriteLock m_uidLock;
    mutable QReadWriteLock m_statusLock;

    Type m_lastMessageType {Type::Unknown};
    QMap<int, QVariant> m_receivedData;
    bool m_started{false};
    bool m_writeOpened{false};
    int m_leftToRead{0};
};

#endif // SERVERWORKER_H
