#include "mainwindow.h"
#include "libvirt_vm_service.h"
#include "manager/vm_watchdog.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    LibvirtVMService service;
    VMWatchdog watchdog(service);

    MainWindow w(service);

    // ウォッチドッグの通知をメインウィンドウへ配線する
    QObject::connect(&watchdog, &VMWatchdog::vmRecovered, &w, &MainWindow::onVmRecovered);
    QObject::connect(&watchdog, &VMWatchdog::vmRecoveryFailed, &w, &MainWindow::onVmRecoveryFailed);

    watchdog.start();       // 配線が済んでから監視開始
    w.show();
    
    return a.exec();
}
