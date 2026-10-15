#pragma once

#include <QObject>
#include <QTimer>
#include <QHash>
#include <QString>

#include "vm_service.h"

/*
* VMウォッチドッグ（マネージャー層）
* 周期的にIVMService経由でVM一覧を取得し、クラッシュしたVMを検出する
* 現段階は「観察のみモード」：検出してログを出すだけで、復旧は行わない
*/

class VMWatchdog : public QObject
{
    Q_OBJECT

public:
    explicit VMWatchdog(IVMService& service, QObject* parent = nullptr);

    void start();                     // 監視を開始する
    void stop();                      // 監視を停止する

signals:
    void vmRecovered(const QString& name);                              // 復旧（起動）に成功した
    void vmRecoveryFailed(const QString& name, const QString& reason);  // 諦めた（上限到達）

private slots:
    void onTick();                    // 周期実行の本体

private:
    int nextIntervalMs() const;       // 次回までのランダム間隔を決める

    IVMService& service_;             // コアへの入口（GUIと同じ注入方式）
    QTimer timer_;                    // 周期タイマー（シングルショットを毎回再始動）
    QHash<QString, int> retryCounts_; // VM名→復旧試行回数（連続失敗の記録）
};


