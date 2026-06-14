#include "ui/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("ESP-FC");
    QCoreApplication::setApplicationName("ESP-FC Configurator");
    QCoreApplication::setApplicationVersion("0.1.0");

    MainWindow window;
    window.resize(1180, 760);
    window.show();

    return app.exec();
}
