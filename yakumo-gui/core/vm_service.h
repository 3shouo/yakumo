
#pragma once

#include <string>
#include <vector>
#include "vm_types.h"

/*
 * コア機能の内部インターフェース。
 * GUI・ヘッドレス・ウォッチドッグ・将来のWebサーバは
 * すべてこのクラス経由でVM操作を行う。
 * 実装クラス（libvirt直結、将来はリモート転送）はこれを継承して作る。
 */
class IVMService {
public:
    virtual ~IVMService() = default;        // 派生クラスを正しく破棄するための仮想デストラクタ

    // VMライフサイクル

    // VMを新規作成する
    virtual VMResult createVM(
        const std::string& name,            // VM名
        unsigned int memoryMB,              // メモリ(MB)
        unsigned int vcpus,                 // vCPU数
        const std::string& diskPath         // qcow2ディスクの絶対パス
    ) = 0;

    // VMを起動する
    virtual VMResult startVM(const std::string& name) = 0;

    // VMをシャットダウンする（ゲストOSに通知する穏当な停止）
    virtual VMResult shutdownVM(const std::string& name) = 0;

    // VMを強制停止する（電源断相当）
    virtual VMResult forceStopVM(const std::string& name) = 0;

    // VMを再起動する
    virtual VMResult rebootVM(const std::string& name) = 0;

    // VMの定義を削除する（稼働中は拒否）
    virtual VMResult deleteVM(const std::string& name) = 0;

    // ---- 情報取得 ----

    // 全VMの一覧を outVms に格納する
    virtual VMResult listVMs(std::vector<VMInfo>* outVms) = 0;

    // VNC接続情報を outInfo に格納する
    virtual VMResult getVncConsoleInfo(
            const std::string& name,
            VncConsoleInfo* outInfo
        ) = 0;

    // ---- スナップショット ----

    // スナップショットを作成する
    virtual VMResult createSnapshot(
            const std::string& vmName,
            const std::string& snapshotName,
            const std::string& description   // 説明（空文字可）
        ) = 0;

    // スナップショット一覧を outSnapshots に格納する
    virtual VMResult listSnapshots(
            const std::string& vmName,
            std::vector<SnapshotInfo>* outSnapshots
        ) = 0;

    // 指定スナップショットへ復元する
    virtual VMResult revertSnapshot(
            const std::string& vmName,
            const std::string& snapshotName
        ) = 0;

    // 指定スナップショットへ復元する
    virtual VMResult deleteSnapshot(
            const std::string& vmName,
            const std::string& snapshotName
        ) = 0;
};

