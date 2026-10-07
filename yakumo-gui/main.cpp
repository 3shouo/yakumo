#include "mainwindow.h"
#include "libvirt_vm_service.h"
#include "manager/vm_watchdog.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    LibvirtVMService service;
    VMWatchdog watchdog(service);
    watchdog.start();

    MainWindow w(service);
    w.show();
    
    return a.exec();
}
