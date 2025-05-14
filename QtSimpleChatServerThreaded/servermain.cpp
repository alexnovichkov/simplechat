#include <QCoreApplication>

#include "server.h"

#include "enums.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    qRegisterMetaType<Enum::MessageType>();
    Server server;
    server.toggleStartServer();
    return a.exec();
}
