#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "vm_types.h"
#include "vm_manager.h"
#include "createvmdialog.h"
#include "vncconsoledialog.h"

#include <QTableWidgetItem>
#include <QDir>
#include <QDebug>
#include <QString>
#include <QTimer>
#include <QMessageBox>
//#include <QInputDialog>
#include <QFileDialog>
#include <functional>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // VM一覧取得
    std::vector<VMInfo> vms = listVMs();
    updateTable(vms);
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this](){
        qDebug() << "timer fired";
        std::vector<VMInfo> vms = listVMs();
        updateTable(vms);
    });
    timer->start(3000);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateTable(const std::vector<VMInfo>& vms)
{
    qDebug() << "updateTable called. size =" << vms.size();

    QString selectedName;
    int currentRow = ui->vmTable->currentRow();
    if (currentRow >= 0 && ui->vmTable->item(currentRow, 0)){
        selectedName = ui->vmTable->item(currentRow, 0)->text();
    }

    ui->vmTable->clearContents();
    ui->vmTable->setRowCount(static_cast<int>(vms.size()));

    for (int row = 0; row < static_cast<int>(vms.size()); ++row) {
        const VMInfo& vm = vms[row];

        ui->vmTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(vm.name)));
        QTableWidgetItem* stateItem = new QTableWidgetItem(stateToString(vm.state));
        ui->vmTable->setItem(row, 1, stateItem);

        if (vm.state == VMState::Running){
            stateItem->setBackground((Qt::green));
        }else if (vm.state == VMState::Shutoff){
            stateItem->setBackground((Qt::lightGray));
        }else if (vm.state == VMState::Paused){
            stateItem->setBackground((Qt::yellow));
        }else{
            stateItem->setBackground((Qt::red));
        }
        ui->vmTable->setItem(row, 2, new QTableWidgetItem(QString::number(vm.memoryMB) + " MB"));
    }
    ui->vmTable->resizeColumnsToContents();

    if (vms.empty()) {
        clearDetail();
        return;
    }

    int rowToSelect = 0;
    for (int row = 0; row < static_cast<int>(vms.size()); ++row){
        if (QString::fromStdString(vms[row].name) == selectedName){
            rowToSelect = row;
            break;
        }
    }

    ui->vmTable->selectRow(rowToSelect);
    updateDetail(vms[rowToSelect]);
}

void MainWindow::updateDetail(const VMInfo &vm)
{
    ui->detailNameValue->setText(QString::fromStdString((vm.name)));
    ui->detailStateValue->setText(stateToString(vm.state));
    ui->detailMemoryValue->setText(QString::number(vm.memoryMB) + "MB");
    ui->detailVcpusValue->setText(QString::number(vm.vcpus));
    ui->detailActiveValue->setText(vm.isActive ? "Yes" : "No");
}

void MainWindow::clearDetail()
{
    ui->detailNameValue->setText("-");
    ui->detailStateValue->setText("-");
    ui->detailMemoryValue->setText("-");
    ui->detailVcpusValue->setText("-");
    ui->detailActiveValue->setText("-");
}

// テーブルで選択中の行からVM名を取り出すヘルパー
QString MainWindow::selectedVMName() const
{
    int row = ui->vmTable->currentRow();
    if (row < 0)
        return QString();

    QTableWidgetItem* item = ui->vmTable->item(row, 0);

    if (!item)
        return QString();

    return item->text();
}

// VM操作ボタン共通の処理本体
void MainWindow::runVMAction(const QString& actionLabel, const std::function<bool(const std::string&)>& action)
{
    QString name = selectedVMName();
    if (name.isEmpty())
        return;

    qDebug() << actionLabel << "row =" << ui->vmTable->currentRow();

    if (!action(name.toStdString())){
        QMessageBox::warning(this, actionLabel + "failed", QString("Failed to %1 VM '%2'.").arg(actionLabel.toLower(), name));
    }

    updateTable(listVMs());
}


void MainWindow::on_vmTable_cellClicked(int row, int column)
{
    Q_UNUSED(column);

    QString name = ui->vmTable->item(row, 0)->text();
    std::vector<VMInfo> vms = listVMs();

    for (const auto& vm : vms) {
        if (QString::fromStdString(vm.name) == name){
            updateDetail(vm);
            return;
        }
    }
}

void MainWindow::on_startButton_clicked()
{
    runVMAction("Start", startVM);
}

void MainWindow::on_shutdownButton_clicked()
{
    runVMAction("Shutdown", shutdownVM);
}

void MainWindow::on_rebootButton_clicked()
{
    runVMAction("Reboot", rebootVM);
}

void MainWindow::on_forceStopButton_clicked()
{
    runVMAction("Forcestop", forceStopVM);
}

void MainWindow::on_deleteButton_clicked()
{
    qDebug() << "row =" << ui->vmTable->currentRow();

    int row = ui->vmTable->currentRow();
    if (row < 0) return;

    QString name = ui->vmTable->item(row, 0)->text();

    auto reply = QMessageBox::question(
        this,
        "Delete VM",
        QString("Delete VM '%1' ?\n This removes the libvirt definition.").arg(name),
        QMessageBox::Yes, QMessageBox::No
        );

    if (reply != QMessageBox::Yes){
        return;
    }

    bool ok = deleteVM(name.toStdString());

    if (!ok) {
        QMessageBox::warning(
            this,
            "Delete failed",
            "Delete failed.\nMake sure the VM is shutoff before deleting."
            );
    }

    std::vector<VMInfo> vms = listVMs();
    updateTable(vms);
}

// VM作成ボタン(createButton)が押されたときの処理
void MainWindow::on_createButton_clicked()
{
    CreateVMDialog dialog(this);

    if (dialog.exec() != QDialog::Accepted){
        return;
    }

    std::string errorMessage;

    bool ok = createVM(
        dialog.vmName().toStdString(),
        static_cast<unsigned int>(dialog.memoryMB()),
        static_cast<unsigned int>(dialog.vcpus()),
        dialog.diskPath().toStdString(),
        &errorMessage
    );

    qDebug() << "createVM result =" << ok;

    if(ok) {
        QMessageBox::information(
            this,
            "Create VM",
            "VM was created successfully."
        );
    } else {
        QMessageBox::warning(
            this,
            "Create VM",
            QString::fromStdString(errorMessage)
        );
    }

    std::vector<VMInfo> vms = listVMs();
    updateTable(vms);
}

// Consoleボタンが押されたときの処理
void MainWindow::on_consoleButton_clicked()
{
    int row = ui->vmTable->currentRow();

    if (row < 0){
        QMessageBox::warning(this, "Console", "Please select a VM.");
        return;
    }

    QTableWidgetItem* nameItem = ui->vmTable->item(row, 0);

    if (!nameItem){
        QMessageBox::warning(this, "Console", "VM name was not found.");
        return;
    }

    QString name = nameItem->text();

    VncConsoleDialog dialog(name, this);
    dialog.exec();
}
