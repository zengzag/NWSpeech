#include <QApplication>
#include <QStyleFactory>
#include <QIcon>
#include "ui/main_window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("NWSpeech");
    app.setOrganizationName("NWSpeech");
    app.setWindowIcon(QIcon(":/icon.ico"));

    app.setStyle(QStyleFactory::create("Fusion"));

    MainWindow window;
    window.show();

    return app.exec();
}
