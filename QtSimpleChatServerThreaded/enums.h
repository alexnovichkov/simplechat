#ifndef ENUMS_H
#define ENUMS_H

#include <QObject>

constexpr int SERVER_PORT = 1967;

enum class MessageType {
    Info,
    Warning,
    Critical
};
Q_DECLARE_METATYPE(MessageType)

#endif // ENUMS_H
