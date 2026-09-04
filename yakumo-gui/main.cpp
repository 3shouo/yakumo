#include "mainwindow.h"
#include "libvirt_vm_service.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    //MainWindow w;
    LibvirtVMService service;
    MainWindow w(service);
    w.show();
    return a.exec();
}
