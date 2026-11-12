
#pragma once

#include <QDialog>
#include <QString>
#include <QFutureWatcher>   // 別スレッドの処理完了をシグナルで受け取る
#include <functional>       // std::function
#include "vm_types.h"       // VMResult（Qt 非依存の共有型。core/ は INCLUDEPATH 済み）

//　前方宣言（ヘッダでは型名だけわかればよい＝コンパイル高速化）
class QTableWidget;
class QPushButton;
class QLabel;               // 処理用メッセージ用
class IVMService;

class SnapshotDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit SnapshotDialog(IVMService& service, const QString& vmName, QWidget* parent = nullptr);

    protected:
        void reject() override;                 // ☓ / Esc / Alt+F4 からの「閉じる」要求を処理中は無視する

    private slots:
        void onCreateClicked();
        void onRevertClicked();
        void onDeleteClicked();
        void onJobFinished();                   // ワーカー完了時に GUI スレッドで呼ばれる

    private:
        void refresh();                         // 一覧を取り直してテーブルへ反映
        QString selectedSnapshotName() const;   // 選択行のスナップショット名
        void startJob(const QString& title, std::function<VMResult()> job);  // 非同期実行の入口（差し替え境界）
        void setBusy(bool busy);

        IVMService&   service_;                 // コアへの入口
        QString       vmName_;                  // 対象VM名
        QTableWidget* table_;                   // 一覧テーブル
        QPushButton*  createButton_;
        QPushButton*  revertButton_;
        QPushButton*  deleteButton_;
        QPushButton*  closeButton_;
        QLabel*       statusLabel_;             // 処理中メッセージ（ボタン行の左側）
        QFutureWatcher<VMResult>* watcher_;     // 完了通知の受け口
        QString       jobTitle_;                // 失敗ダイアログのタイトル用
        bool          busy_ = false;            // 処理中フラグ
};


