#include "vm_watchdog.h"

#include <QDebug>
#include <QRandomGenerator>
#include <QString>
#include <vector>

// 監視周期（15~25秒のランダム。GUIの70~90秒とは別周期にして位相衝突を避ける）
static constexpr int kTickMinMs = 15 * 1000;
static constexpr int kTickMaxMs = 25 * 1000;
static constexpr int kMaxRetries = 3;           // 同一VMへの復旧試行回数の上限

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

    for (const VMInfo& vm : vms) {
            const QString name = QString::fromStdString(vm.name);

            // 正常稼働中のVMは失敗カウンタをリセット（復旧が定着した、または手動で直された）
            if (vm.state == VMState::Running && !vm.crashed) {
                retryCounts_.remove(name);
                continue;
            }

            if (!vm.crashed) {
                continue;               // クラッシュ以外の停止（正常停止・ForceStopなど）には触れない
            }

            int attempts = retryCounts_.value(name, 0);     // これまでの試行回数（未登録なら0）

            // 上限に達したVMは諦める（通知は一度だけ出し、以降は静観）
            if (attempts >= kMaxRetries) {
                if (attempts == kMaxRetries) {
                    qDebug() << "[watchdog] giving up on" << name;
                    emit vmRecoveryFailed(name, QString("Gave up after %1 recovery attempts").arg(kMaxRetries));
                    retryCounts_[name] = kMaxRetries + 1;       // 「通知済み」の印として1つ勧めておく
                }
            continue;
        }

        // 復旧を試みる（試行回数は成否に関わらず数える）
        retryCounts_[name] = attempts + 1;
        qDebug() << "[watchdog] recovering" << name
                 << "(attempt" << attempts + 1 << "/" << kMaxRetries << ")";

        VMResult startResult = service_.startVM(vm.name);

        if (startResult.ok) {
            qDebug() << "[watchdog] recovery started:" << name;
            emit vmRecovered(name);
        } else {
            qDebug() << "[watchdog] recovery failed:" << name
                     << QString::fromStdString(startResult.message);
        }
    }
    timer_.start(nextIntervalMs());       // 次回を予約
}

