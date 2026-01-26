#include "mainwindow.hpp"

#include <QApplication>

int main(int argc, char* argv[])
{
    QCoreApplication::setOrganizationName("AltairX");
    QCoreApplication::setApplicationName("AltairXVM");
    
    QApplication app{argc, argv};

    MainWindow window{};
    window.show();

    return app.exec();
}
