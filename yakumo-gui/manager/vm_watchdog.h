#pragma once

#include <QObject>
#include <QTimer>

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

private slots:
    void onTick();                    // 周期実行の本体

private:
    int nextIntervalMs() const;       // 次回までのランダム間隔を決める

    IVMService& service_;             // コアへの入口（GUIと同じ注入方式）
    QTimer timer_;                    // 周期タイマー（シングルショットを毎回再始動）
};


