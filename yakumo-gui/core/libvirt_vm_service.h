
#pragma once

#include "vm_service.h"

/*
 * IVMService の libvirt 実装。
 * 現段階では既存の自由関数（vm_manager.cpp / snapshot_manager.cpp）へ
 * 委譲するだけの薄いラッパー。将来、自由関数の中身をここへ移して一本化する。
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

