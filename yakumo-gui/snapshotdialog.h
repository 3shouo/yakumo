
#pragma once

#include <QDialog>
#include <QString>

//　前方宣言（ヘッダでは型名だけわかればよい＝コンパイル高速化）
class QTableWidget;
class QPushButton;

class SnapshotDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit SnapshotDialog(const QString& vmName, QWidget* parent = nullptr);

    private slots:
        void onCreateClicked();
        void onRevertClicked();
        void onDeleteClicked();

    private:
        void refresh();                         // 一覧を取り直してテーブルへ反映
        QString selectedSnapshotName() const;   // 選択行のスナップショット名

        QString       vmName_;                  // 対象VM名
        QTableWidget* table_;                   // 一覧テーブル
        QPushButton*  createButton_;
        QPushButton*  revertButton_;
        QPushButton*  deleteButton_;
        QPushButton*  closeButton_;
};


