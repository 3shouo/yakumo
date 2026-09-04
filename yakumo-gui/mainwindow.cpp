#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "vm_types.h"
#include "createvmdialog.h"
#include "snapshotdialog.h"
#include "vncconsoledialog.h"

#include <QTableWidgetItem>
#include <QDir>
#include <QDebug>
#include <QString>
#include <QTimer>
#include <QMessageBox>
#include <QFileDialog>
#include <functional>
#include <QRandomGenerator>


MainWindow::MainWindow(IVMService& service, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , service_(service)
{
    ui->setupUi(this);

    // VM一覧取得
    std::vector<VMInfo> vms = fetchVMs();
    updateTable(vms);

    // 状態更新の間隔（80秒±10秒 → 70〜90秒）
    constexpr int kUpdateMinMs = 70 * 1000;
    constexpr int kUpdateMaxMs = 90 * 1000;

    // 次回の更新間隔をランダムに決めるヘルパー
    auto nextInterval = []() {
        return QRandomGenerator::global()->bounded(kUpdateMinMs, kUpdateMaxMs + 1);
    };

    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);         // 繰り返しではなく1回だけ発火するモード
    connect(timer, &QTimer::timeout, this, [this, timer, nextInterval](){
        qDebug() << "timer fired";
        std::vector<VMInfo> vms = fetchVMs();
        updateTable(vms);
        timer->start(nextInterval());   // 毎回新しいランダム間隔で再スタート
    });
    timer->start(nextInterval());
}

MainWindow::~MainWindow()
{
    delete ui;
}

// コアからVM一覧を取得するヘルパー（失敗時は空の一覧を返す）
std::vector<VMInfo> MainWindow::fetchVMs()
{
    std::vector<VMInfo> vms;                        // 出力先の入れ物
    VMResult result = service_.listVMs(&vms);       // インターフェース経由で取得
    if(!result.ok) {
        qDebug() << "listVMs failed : " << QString::fromStdString(result.message);
    }
    return vms;
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
void MainWindow::runVMAction(const QString& actionLabel, const std::function<VMResult(const std::string&)>& action)
{
    QString name = selectedVMName();
    if (name.isEmpty())
        return;

    qDebug() << actionLabel << "row =" << ui->vmTable->currentRow();

    VMResult result = action(name.toStdString());                   // 操作を実行して結果を受ける
    if(!result.ok) {
        QMessageBox::warning(this, actionLabel + " failed", QString::fromStdString(result.message));    // コアからの実際のエラー文言を表示
    }
    /*
    if (!action(name.toStdString())){
        QMessageBox::warning(this, actionLabel + "failed", QString("Failed to %1 VM '%2'.").arg(actionLabel.toLower(), name));
    }
    */

    updateTable(fetchVMs());
}


void MainWindow::on_vmTable_cellClicked(int row, int column)
{
    Q_UNUSED(column);

    QString name = ui->vmTable->item(row, 0)->text();
    std::vector<VMInfo> vms = fetchVMs();

    for (const auto& vm : vms) {
        if (QString::fromStdString(vm.name) == name){
            updateDetail(vm);
            return;
        }
    }
}

void MainWindow::on_startButton_clicked()
{
    runVMAction("Start", [this](const std::string& name){ return service_.startVM(name); });
}

void MainWindow::on_shutdownButton_clicked()
{
    runVMAction("Shutdown", [this](const std::string& name){ return service_.shutdownVM(name); });
}

void MainWindow::on_rebootButton_clicked()
{
    runVMAction("Reboot", [this](const std::string& name){ return service_.rebootVM(name); });
}

void MainWindow::on_forceStopButton_clicked()
{
    runVMAction("ForceStop", [this](const std::string& name){ return service_.forceStopVM(name); });
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

    VMResult result = service_.deleteVM(name.toStdString());

    if (!result.ok) {
        QMessageBox::warning(
            this,
            "Delete failed",
            QString::fromStdString(result.message)
        );
    }

    std::vector<VMInfo> vms = fetchVMs();
    updateTable(vms);
}

// VM作成ボタン(createButton)が押されたときの処理
void MainWindow::on_createButton_clicked()
{
    CreateVMDialog dialog(this);

    if (dialog.exec() != QDialog::Accepted){
        return;
    }

    VMResult result = service_.createVM(
        dialog.vmName().toStdString(),
        static_cast<unsigned int>(dialog.memoryMB()),
        static_cast<unsigned int>(dialog.vcpus()),
        dialog.diskPath().toStdString()
    );

    qDebug() << "createVM result =" << result.ok;

    if(result.ok) {
        QMessageBox::information(
            this,
            "Create VM",
            "VM was created successfully."
        );
    } else {
        QMessageBox::warning(
            this,
            "Create VM",
            QString::fromStdString(result.message)
        );
    }

    std::vector<VMInfo> vms = fetchVMs();
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

    VncConsoleDialog dialog(service_, name, this);
    dialog.exec();
}


// Snapshotボタンが押されたときの処理
void MainWindow::on_snapshotButton_clicked()
{
    int row = ui->vmTable->currentRow();

    if (row < 0) {
        QMessageBox::warning(this, "Snapshot", "Please select a VM.");
        return;
    }

    QTableWidgetItem* nameItem = ui->vmTable->item(row, 0);

    if (!nameItem) {
        QMessageBox::warning(this, "snapshot", "VM name was not found.");
        return;
    }

    QString name = nameItem->text();

    SnapshotDialog dialog(service_, name, this);  // 選択中VM名を渡して生成
    dialog.exec();                                // モーダル表示（閉じるまでブロック）
}
