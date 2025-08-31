#ifndef ENUMS_H
#define ENUMS_H

#include <QObject>
#include <QVariant>
#include <QMap>

constexpr int SERVER_PORT = 1967;

enum class MessageType {
    Info,
    Warning,
    Critical
};
Q_DECLARE_METATYPE(MessageType)

enum Type {
    SenderName,//string //кто отправил сообщение
    SenderUid,//string  //кто отправил сообщение
    ReceiverName,//string //кто получает сообщение
    ReceiverUid,//string  //кто получает сообщение
    DataType,//string
    UserName, //string //имя нового игрока или отключившегося игрока
    UserUid, //string  //uid нового игрока или отключившегося игрока
    Success, //bool
    Reason, //string
    Users,//list
    Unknown = 65535
};

// using DataList = QMap<int, QVariant>;

#endif // ENUMS_H
