
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
#include <QtConcurrent>         // QtConcurrent::run（名前空間 QtConcurrent を宣言する）
#include <QFuture>              // QFuture<T>（QFutureWatcher 経由でも入るが、使う型は明示しておく）
#include <QLabel>

SnapshotDialog::SnapshotDialog(IVMService& service, const QString& vmName, QWidget* parent)
    : QDialog(parent)
    , service_(service)
    , vmName_(vmName)
    , table_(new QTableWidget(this))
    , createButton_(new QPushButton("Create", this))
    , revertButton_(new QPushButton("Revert", this))
    , deleteButton_(new QPushButton("Delete", this))
    , closeButton_(new QPushButton("Close", this))
    , statusLabel_(new QLabel(this))                        // 空文字で生成し、処理中だけ文字を入れる
    , watcher_(new QFutureWatcher<VMResult>(this))          // 親を this にして寿命をダイアログに任せる
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
    buttonLayout->addWidget(statusLabel_);      // Delete の右に処理中メッセージ
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
    connect(watcher_, &QFutureWatcher<VMResult>::finished, this, &SnapshotDialog::onJobFinished);   // 完了通知を配線

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
    table_->clearSelection();                                   // 一覧を作り直したら選択を外す（別の行が選ばれたままになるのを防ぐ）
}

// 選択行の名前の取得
QString SnapshotDialog::selectedSnapshotName() const
{
    // 選択行（ハイライトされている行）の名前の取得
    QList<QTableWidgetItem*> items = table_->selectedItems();     // ハイライト中の説の一覧を取得

    if (items.isEmpty()) {
        return QString();        // 未選択
    }

    int row = items.first()->row();                            // 先頭セルから行番号を得る（行単位・単一選択なので全セル同じ行）
    QTableWidgetItem* item = table_->item(row, 0);             // その行の0列目(Name)

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

    if (!ok){
        return;             // キャンセル（Cancel / Esc / ☓）は何もしない
    }

    name = name.trimmed();  // 前後の空白を除去（スペースだけの入力も空として扱う）

    if (name.isEmpty()) {
        QMessageBox::warning(this, "Create Snapshot", "Please enter a snapshot name.");
        return;             // 空入力は理由を示して中断
    }

    IVMService* svc  = &service_;                    // ワーカーへ渡す参照先をポインタで取り出す（thisは渡さない）
    std::string vm   = vmName_.toStdString();        // QString はダイアログの寿命で縛られるので std::string にコピー
    std::string snap = name.toStdString();              // nameはこの関数を抜けると消えるローカル変数なので同様にコピー

    startJob("Create Snapshot", [svc, vm, snap]() {
        return svc->createSnapshot(vm, snap, "");    // この行だけが別スレッドで動く
    });
    // 結果表示とrefresh()は完了時にonJobFinished()が行う
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

    IVMService* svc  = &service_;                    // ワーカーへ渡す参照をポインタで取り出す（thisは渡さない）
    std::string vm   = vmName_.toStdString();        // QString はダイアログの寿命に縛られるのでstd::stringにコピー
    std::string snap = name.toStdString();           // name はこの関数を抜けると消えるので同様にコピー

    startJob("Revert Snapshot", [svc, vm, snap]() {
        return svc->revertSnapshot(vm, snap);        // この行だけが別スレッド
    });
    // 結果表示と refresh() は完了時に onJobFinished() が行う
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

    IVMService* svc  = &service_;                    // ワーカーへ渡す参照をポインタで取り出す（thisは渡さない）
    std::string vm   = vmName_.toStdString();        // QString はダイアログの寿命に縛られるのでstd::stringにコピー
    std::string snap = name.toStdString();           // name はこの関数を抜けると消えるので同様にコピー

    startJob("Delete Snapshot", [svc, vm, snap]() {
        return svc->deleteSnapshot(vm, snap);        // この行だけが別スレッド
        });
    // 結果表示と refresh() は完了時に onJobFinished() が行う
}

// 非同期ジョブを開始する（スレッド機構はこの関数と onJobFinished に閉じ込める）
void SnapshotDialog::startJob(const QString& title, std::function<VMResult()> job)
{
    jobTitle_ = title;                                      // 失敗時のダイアログタイトルを控える
    setBusy(true);                                          // 多重実行を防ぐため先にボタンを無効化
    QFuture<VMResult> future = QtConcurrent::run(job);      // スレッドプールの別スレッドで job() を開始
    watcher_->setFuture(future);                            // この future の完了を見張る → finishedが飛ぶ
}


// ワーカー完了後、GUIスレッドで呼ばれる
void SnapshotDialog::onJobFinished()
{
    VMResult result = watcher_->result();                   // 戻り値を取り戻す（完了済みなので待たない）
    setBusy(false);                                         // ボタンを戻す

    if (!result.ok) {
        QMessageBox::warning(this, jobTitle_, QString::fromStdString(result.message));
    }

    refresh();                                             // 成否に関わらず最新化（従来と同じ）
}

// 処理中はボタンと一覧を触れないようにし、状態を表示する
void SnapshotDialog::setBusy(bool busy)
{
    busy_ = busy;
    createButton_->setEnabled(!busy);
    revertButton_->setEnabled(!busy);
    deleteButton_->setEnabled(!busy);
    closeButton_->setEnabled(!busy);
    table_->setEnabled(!busy);                              // 処理中に選択行を変えられないようにする

    if (busy) {
        statusLabel_->setText(jobTitle_ + " in progress...");   // 例: "Create Snapshot in progress.........."
        setCursor(Qt::BusyCursor);                          // このダイアログ上では「作業中」カーソル
    } else {
        statusLabel_->clear();                              // 文字を消す
        unsetCursor();                                      // 設定のカーソルに戻す
    }
}

// 閉じる要求（☓ / Esc / Alt+F4）。処理中は無視する
void SnapshotDialog::reject()
{
    if (busy_) {
        return;                                             // ワーカーが動いている間はダイアログ閉じない
    }
    QDialog::reject();                                      // 通常通り閉じる（exec(） が Rejected で戻る）
}

