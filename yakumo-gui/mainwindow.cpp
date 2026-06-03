#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "vm_types.h" // 2026/1/19 追加
#include "vm_manager.h" // 2026/1/19 追加

#include <QTableWidgetItem> // 2026/1/19 追加
#include <QDir>
#include <QDebug>
#include <QString>
#include <QTimer>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // VM一覧取得
    std::vector<VMInfo> vms = listVMs();
    updateTable(vms);     // 2026/1/19 追加
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

void MainWindow::updateTable(const std::vector<VMInfo>& vms) // 2026/1/19 追加
{
    qDebug() << "updateTable called. size =" << vms.size();

    ui->vmTable->clearContents();
    ui->vmTable->setRowCount(static_cast<int>(vms.size()));

    for (int row = 0; row < static_cast<int>(vms.size()); ++row) {
        const VMInfo& vm = vms[row];

        ui->vmTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(vm.name)));
        QTableWidgetItem* stateItem = new QTableWidgetItem(stateToString(vm.state));
        ui->vmTable->setItem(row, 1, stateItem);

        if (vm.state == VMState::Running)
        {
            stateItem->setBackground((Qt::green));
        }
        else if (vm.state == VMState::Shutoff)
        {
            stateItem->setBackground((Qt::lightGray));
        }
        else if (vm.state == VMState::Paused)
        {
            stateItem->setBackground((Qt::yellow));
        }
        else
        {
            stateItem->setBackground((Qt::red));
        }
        ui->vmTable->setItem(row, 2, new QTableWidgetItem(QString::number(vm.memoryMB) + " MB"));
    }
    ui->vmTable->resizeColumnsToContents();

    if (!vms.empty()) {
        ui->vmTable->selectRow(0);
        updateDetail(vms[0]);
    } else {
        clearDetail();
    }

    if (ui->vmTable->rowCount() > 0)
    {
        ui->vmTable->selectRow(0);
    }
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

void MainWindow::on_rebootButton_clicked()
{
    qDebug() << "row =" << ui->vmTable->currentRow();

    int row = ui->vmTable->currentRow();
    if (row < 0) return;

    QString name = ui->vmTable->item(row, 0)->text();

    rebootVM(name.toStdString());

    std::vector<VMInfo> vms = listVMs();
    updateTable(vms);
}

void MainWindow::on_forceStopButton_clicked()
{
    qDebug() << "row =" << ui->vmTable->currentRow();

    int row = ui->vmTable->currentRow();
    if (row < 0) return;

    QString name = ui->vmTable->item(row, 0)->text();

    forceStopVM(name.toStdString());

    std::vector<VMInfo> vms = listVMs();
    updateTable(vms);
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
    bool okInput = false;

    QString name = QInputDialog::getText(
        this,
        "Create VM",
        "VM name:",
        QLineEdit::Normal,
        "",
        &okInput
    );

    if (!okInput || name.isEmpty()) {
        return;
    }

    int memoryMB = QInputDialog::getInt(
        this,
        "Create VM",
        "Memory MB:",
        2048,
        256,
        32768,
        256,
        &okInput
    );

    if (!okInput) {
        return;
    }

    int vcpus = QInputDialog::getInt(
        this,
        "Create VM",
        "vCPUs",
        2,
        1,
        16,
        1,
        &okInput
    );

    if (!okInput) {
        return;
    }


    QString diskPath = QFileDialog::getOpenFileName(
        this,
        "Select qcow2 Disk Image",
        QDir::homePath(),
        "QCOW2 Image (*.qcow2)"
    );

    if (diskPath.isEmpty()) {
        return;
    }


    std::string errorMessage;

    bool ok = createVM(
        name.toStdString(),
        static_cast<unsigned int> (memoryMB),
        static_cast<unsigned int>(vcpus),
        diskPath.toStdString(),
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
