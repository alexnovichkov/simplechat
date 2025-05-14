#ifndef ENUMS_H
#define ENUMS_H

#include <QObject>

constexpr int SERVER_PORT = 1967;

namespace Enum {
    Q_NAMESPACE
    enum MessageType {
        Info,
        Warning,
        Critical
    };
    Q_ENUM_NS(MessageType);
}

#endif // ENUMS_H
