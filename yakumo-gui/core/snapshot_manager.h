
#pragma once

#include <vector>
#include <string>
#include "vm_types.h"


// スナップショット作成
bool createSnapshot(
    const std::string& vmName,
    const std::string& snapshotName,
    const std::string& description,
    std::string* errorMessage = nullptr
);

// スナップショット一覧取得
std::vector<SnapshotInfo> listSnapshots(
    const std::string& vmName,
    std::string* errorMessage = nullptr
);

// スナップショットへ復元
bool revertSnapshot(
    const std::string& vmName,
    const std::string& snapshotName,
    std::string* errorMessage = nullptr
);

// スナップショット削除
bool deleteSnapshot(
    const std::string& vmName,
    const std::string& snapshotname,
    std::string* errorMessage = nullptr
);

