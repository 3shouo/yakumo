
#include "snapshotdialog.h"
#include "vm_service.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QDateTime>
#include <QStringList>
#include <vector>

SnapshotDialog::SnapshotDialog(IVMService& service, const QString& vmName, QWidget* parent)
    : QDialog(parent)
    , service_(service)
    , vmName_(vmName)
    , table_(new QTableWidget(this))
    , createButton_(new QPushButton("Create", this))
    , revertButton_(new QPushButton("Revert", this))
    , deleteButton_(new QPushButton("Delete", this))
    , closeButton_(new QPushButton("Close", this))
{
    setWindowTitle("Snapshot - " + vmName);
    resize(640, 360);

    // --- テーブル設定（5列、編集不可、行単位で単一選択） ---
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels(QStringList() << "Name" << "State" << "Create" << "Parent" << "Current");
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->horizontalHeader()->setStretchLastSection(true);

    // --- ボタン行（Create/Revert/Delete は左、CLose は右端） ---
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(createButton_);
    buttonLayout->addWidget(revertButton_);
    buttonLayout->addWidget(deleteButton_);
    buttonLayout->addStretch();                 // 伸縮スペースで Close を右へ押しやる
    buttonLayout->addWidget(closeButton_);

    // --- 全体レイアウト（上：テーブル / 下：ボタン行） ---
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(table_);
    mainLayout->addLayout(buttonLayout);

    // --- シグナル接続 ---
    connect(createButton_, &QPushButton::clicked, this, &SnapshotDialog::onCreateClicked);
    connect(revertButton_, &QPushButton::clicked, this, &SnapshotDialog::onRevertClicked);
    connect(deleteButton_, &QPushButton::clicked, this, &SnapshotDialog::onDeleteClicked);
    connect(closeButton_,  &QPushButton::clicked, this, &QDialog::accept);

    refresh(); // 初回一覧表示
}


// 一覧の取得と描画を行う
void SnapshotDialog::refresh()
{
    std::vector<SnapshotInfo> snaps;
    VMResult result = service_.listSnapshots(vmName_.toStdString(), &snaps);

    if (!result.ok) {
        QMessageBox::warning(this, "Snapshots", QString::fromStdString(result.message));
    }

    table_->setRowCount(static_cast<int>(snaps.size()));

    for (int row = 0; row < static_cast<int>(snaps.size()); ++row) {
        const SnapshotInfo& s = snaps[row];

        // エポック秒　→　ローカル日時文字列
        QString created = QDateTime::fromSecsSinceEpoch(s.creationTime).toString("yyyy-MM-dd HH:mm:ss");

        table_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(s.name)));
        table_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(s.state)));
        table_->setItem(row, 2, new QTableWidgetItem(created));
        table_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(s.parent)));
        table_->setItem(row, 4, new QTableWidgetItem(s.isCurrent ? "★" : ""));
    }

    table_->resizeColumnsToContents();
    table_->horizontalHeader()->setStretchLastSection(true);
}

// 選択行の名前の取得
QString SnapshotDialog::selectedSnapshotName() const
{
    int row = table_->currentRow();

    if (row < 0) {
        return QString();       // 未選択
    }

    QTableWidgetItem* item = table_->item(row, 0);
    
    if (!item) {
        return QString();
    }
    return item->text();
}

// QInputDialogで名前入力
void SnapshotDialog::onCreateClicked()
{
    bool ok = false;

    QString name = QInputDialog::getText(this, "Create Snapshot", "Snapshot name:", QLineEdit::Normal, QString(), &ok);

    if (!ok || name.isEmpty()){
        return;             // キャンセル or 空入力
    }

    /*
    std::string errorMessage;
    bool created = createSnapshot(vmName_.toStdString(), name.toStdString(), "", &errorMessage);

    if (!created) {
        QMessageBox::warning(this, "Create Snapshot", QString::fromStdString(errorMessage));
    }
    */

    VMResult result = service_.createSnapshot(vmName_.toStdString(), name.toStdString(), "");

    if (!result.ok) {
        QMessageBox::warning(this, "Create Snapshot", QString::fromStdString(result.message));
    }

    refresh();              // 成否に関わらず最新化
}


void SnapshotDialog::onRevertClicked()
{
    QString name = selectedSnapshotName();

    if (name.isEmpty()) {
        QMessageBox::warning(this, "Revert", "Please select a snapshot.");
        return;
    }

    auto reply = QMessageBox::question(this, "Revert Snapshot", QString("Revert VM '%1' to snapshot '%2'?\n" "Current state will be lost.").arg(vmName_, name), QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    /*
    std::string errorMessage;
    bool ok = revertSnapshot(vmName_.toStdString(), name.toStdString(), &errorMessage);

    if (!ok) {
        QMessageBox::warning(this, "Revert Snapshot", QString::fromStdString(errorMessage));
    }
        */

    VMResult result =service_.revertSnapshot(vmName_.toStdString(), name.toStdString());

    if (!result.ok) {
        QMessageBox::warning(this, "Revert Snapshot", QString::fromStdString(result.message));
    }

    refresh();
}


void SnapshotDialog::onDeleteClicked()
{
    QString name = selectedSnapshotName();

    if (name.isEmpty()) {
        QMessageBox::warning(this, "Delete", "Please select a snapshot.");
        return;
    }

    auto reply = QMessageBox::question(this, "Delete Snapshot", QString("Delete snapshot '%1'?").arg(name), QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    /*
    std::string errorMessage;
    bool ok = deleteSnapshot(vmName_.toStdString(), name.toStdString(), &errorMessage);

    if (!ok) {
        QMessageBox::warning(this, "Delete Snapshot", QString::fromStdString(errorMessage));
    }
    */

    VMResult result = service_.deleteSnapshot(vmName_.toStdString(), name.toStdString());

    if (!result.ok) {
        QMessageBox::warning(this, "Delete Snapshot", QString::fromStdString(result.message));
    }

    refresh();
}


