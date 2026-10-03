
#pragma once

#include "vm_service.h"

/*
 * IVMService の libvirt 実装。
 * Libvirt C API を直接呼び出す場所（接続、ドメイン操作、XMLの組み立てと解析）
 * エラーはすべて VMResult に理由付きで載せて返す
 */

 class LibvirtVMService : public IVMService {
public:

        // ---- VMライフサイクル ----
        VMResult createVM(
            const std::string& name,
            unsigned int memoryMB,
            unsigned int vcpus,
            const std::string& diskPath
        ) override;
        VMResult startVM(const std::string& name) override;
        VMResult shutdownVM(const std::string& name) override;
        VMResult forceStopVM(const std::string& name) override;
        VMResult rebootVM(const std::string& name) override;
        VMResult deleteVM(const std::string& name) override;

        // ---- 情報取得 ----
        VMResult listVMs(std::vector<VMInfo>* outVms) override;
        VMResult getVncConsoleInfo(const std::string& name, VncConsoleInfo* outInfo) override;

        // ---- スナップショット ----
        VMResult createSnapshot(
            const std::string& vmName,
            const std::string& snapshotName,
            const std::string& description
        ) override;
        VMResult listSnapshots(const std::string& vmName, std::vector<SnapshotInfo>* outSnapshots) override;
        VMResult revertSnapshot(const std::string& vmName, const std::string& snapshotName) override;
        VMResult deleteSnapshot(const std::string& vmName, const std::string& snapshotName) override;
 };

