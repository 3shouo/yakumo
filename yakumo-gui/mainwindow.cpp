#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "vm_types.h" // 2026/1/19 追加
#include "vm_manager.h" // 2026/1/19 追加

#include <QTableWidgetItem> // 2026/1/19 追加
#include <QDebug>

static QString stateToString(VMState state) // 2026/1/19 追加
{
    switch (state) {
        case VMState::Running: return "Running";
        case VMState::Shutoff: return "Shutoff";
        case VMState::Paused:  return "Paused";
        default: return "Unknown";
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // VM一覧取得
    //auto vms = listVMs(); // 2026/1/19 追加
    std::vector<VMInfo> vms = listVMs();
    updateTable(vms);     // 2026/1/19 追加
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateTable(const std::vector<VMInfo>& vms) // 2026/1/19 追加
{
    qDebug() << "updateTable called. size =" << vms.size();

    ui->vmTable->clearContents();
    ui->vmTable->setRowCount(static_cast<int>(vms.size()));

    //std::vector<VMInfo> vms = listVMs();
    //ui->vmTable->setRowCount(vms.size());

    for (int row = 0; row < static_cast<int>(vms.size()); ++row) {
        const VMInfo& vm = vms[row];

        ui->vmTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(vm.name)));
        ui->vmTable->setItem(row, 1, new QTableWidgetItem(stateToString(vm.state)));
        ui->vmTable->setItem(row, 2, new QTableWidgetItem(QString::number(vm.memoryMB) + " MB"));
    }
    ui->vmTable->resizeColumnsToContents();

    if (ui->vmTable->rowCount() > 0)
    {
        ui->vmTable->selectRow(0);
    }
}

void MainWindow::on_startButton_clicked()
{
    qDebug() << "row =" << ui->vmTable->currentRow();

    int row = ui->vmTable->currentRow();

    if(row < 0)
        return;

    QString name = ui->vmTable->item(row, 0)->text();

    startVM(name.toStdString());

    std::vector<VMInfo> vms = listVMs();
    updateTable(vms);
}

void MainWindow::on_shutdownButton_clicked()
{
    qDebug() << "row =" << ui->vmTable->currentRow();

    int row = ui->vmTable->currentRow();
    if (row < 0)
        return;

    QString name = ui->vmTable->item(row, 0)->text();

    shutdownVM(name.toStdString());

    std::vector<VMInfo> vms = listVMs();
    updateTable(vms);
}
