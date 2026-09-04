#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <vector>
#include <QString>
#include <functional>

#include "vm_types.h" 
#include "vm_service.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    //MainWindow(QWidget *parent = nullptr);
    explicit MainWindow(IVMService& service, QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    IVMService& service_;                                   // コアへの入口

    std::vector<VMInfo> fetchVMs();                         // VM一覧取得ヘルパー
    void updateTable(const std::vector<VMInfo>& vms);
    void updateDetail(const VMInfo& vm);
    void clearDetail();
    QString selectedVMName() const;
    //void runVMAction(const QString& actionLabel, const std::function<bool(const std::string&)>& action);
    void runVMAction(const QString& actionLabel, const std::function<VMResult(const std::string&)>& action); // 失敗時にエラー文言をダイアログ表示できるようにする

private slots:
    void on_startButton_clicked();
    void on_shutdownButton_clicked();
    void on_rebootButton_clicked();
    void on_forceStopButton_clicked();
    void on_deleteButton_clicked();
    void on_createButton_clicked();
    void on_consoleButton_clicked();
    void on_snapshotButton_clicked();
    void on_vmTable_cellClicked(int row, int column);
};
#endif // MAINWINDOW_H
