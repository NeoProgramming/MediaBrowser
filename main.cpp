#include "mediabrowser.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MediaBrowser w;
    w.show();
    return a.exec();
}
