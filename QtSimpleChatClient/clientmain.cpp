#include <QApplication>
#include "chatwindow.h"
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName(QStringLiteral("SimpleChatClient"));
    a.setOrganizationName(QStringLiteral("Novichkov"));
    ChatWindow chatWin;
    chatWin.show();
    return a.exec();
}
