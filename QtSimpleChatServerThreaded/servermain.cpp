#include <QCoreApplication>
#include <QDebug>

#include "server.h"
#include "enums.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    qRegisterMetaType<MessageType>();
    qRegisterMetaType<QMap<int, QVariant>>();
    Server server;
    server.toggleStartServer();
    return a.exec();
}
