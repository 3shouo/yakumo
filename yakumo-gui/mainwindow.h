#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <vector>
#include "vm_types.h" // 2026/1/19 追加

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    void updateTable(const std::vector<VMInfo>& vms); // 2026/1/19 追加

private slots:
    void on_startButton_clicked();
    void on_shutdownButton_clicked();
    void on_rebootButton_clicked();
};
#endif // MAINWINDOW_H
