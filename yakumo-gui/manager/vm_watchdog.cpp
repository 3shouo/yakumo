#include "vm_watchdog.h"

#include <QDebug>
#include <QRandomGenerator>
#include <QString>
#include <vector>

// 監視周期（15~25秒のランダム。GUIの70~90秒とは別周期にして位相衝突を避ける）
static constexpr int kTickMinMs = 15 * 1000;
static constexpr int kTickMaxMs = 25 * 1000;

VMWatchdog::VMWatchdog(IVMService& service, QObject* parent)
    : QObject(parent)
    , service_(service)
{
        timer_.setSingleShot(true);             // 1回発火ごとに新しいランダム間隔で再始動する方式
        connect(&timer_, &QTimer::timeout, this, &VMWatchdog::onTick);
}

void VMWatchdog::start()
{
    qDebug() << "[watchdog] started";
    timer_.start(nextIntervalMs());
}

void VMWatchdog::stop()
{
    timer_.stop();
    qDebug() << "[watchdog] stopped";
}

// 15~25秒のランダムな待ち時間を返す
int VMWatchdog::nextIntervalMs() const
{
    return QRandomGenerator::global()->bounded(kTickMinMs, kTickMaxMs + 1);
}

void VMWatchdog::onTick()
{
    std::vector<VMInfo> vms;
    VMResult result = service_.listVMs(&vms);

    if (!result.ok) {
        // 一覧が取れないときは何もせず、次の周期で再挑戦する
        qDebug() << "[watchdog] listVMs failed:" << QString::fromStdString(result.message);
        timer_.start(nextIntervalMs());
        return;
    }

    qDebug() << "[watchdog] tick: checked" << vms.size() << "VMs";      // 動作確認用

    for (const VMInfo& vm : vms) {
        if (vm.crashed) {
            // 現段階は観察のみ：検出をログに残すだけ
            qDebug() << "[watchdog] crashed VM Detected:" << QString::fromStdString(vm.name);
        }
    }

    timer_.start(nextIntervalMs());       // 次回を予約
}

