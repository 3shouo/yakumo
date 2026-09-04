
#include "libvirt_vm_service.h"

#include "vm_manager.h"             // 既存の自由関数（createVM, startVM, listVMs など）
#include "snapshot_manager.h"       // 既存のスナップショット自由関数

// このファイル内だけで使う補助関数
namespace {

// 「bool + エラー文字列」の旧形式を VMResult に変換する
VMResult toResult(bool ok, const std::string& errorMessage, const std::string& fallback)
{
    if (ok) {
        return VMResult::success();     // 成功ならエラー文言は不要
    }
    // 旧関数が文言をくれなかった場合は fallback（汎用メッセージ）を使う
    return VMResult::failure(errorMessage.empty() ? fallback : errorMessage);
}

} // namespace

// ---- VMライフサイクル ----

VMResult LibvirtVMService::createVM(
    const std::string& name,
    unsigned int memoryMB,
    unsigned int vcpus,
    const std::string& diskPath)
{
    std::string errorMessgae;           // 旧形式のエラー受取用
    bool ok = ::createVM(name, memoryMB, vcpus, diskPath, &errorMessgae); // 既存の自由関数へ委譲
    return toResult(ok, errorMessgae, "VMの作成に失敗しました");
}

VMResult LibvirtVMService::startVM(const std::string& name)
{
    // 旧関数はエラー文言未対応なので、失敗時は汎用エラーメッセージを返す
    return toResult(::startVM(name), "", "VMの起動に失敗しました: " + name);
}

VMResult LibvirtVMService::shutdownVM(const std::string& name)
{
    return toResult(::shutdownVM(name), "", "VMのシャットダウンに失敗しました: " + name);
}

VMResult LibvirtVMService::forceStopVM(const std::string& name)
{
    return toResult(::forceStopVM(name), "", "VMの強制停止に失敗しました: " + name);
}

VMResult LibvirtVMService::rebootVM(const std::string& name)
{
    return toResult(::rebootVM(name), "", "VMの再起動に失敗しました: " + name);
}

VMResult LibvirtVMService::deleteVM(const std::string& name)
{
    std::string errorMessage;
    bool ok = ::deleteVM(name, &errorMessage);
    return toResult(ok, errorMessage, "VMの削除に失敗しました: " + name);
}

// ---- 情報取得 ----

VMResult LibvirtVMService::listVMs(std::vector<VMInfo>* outVms)
{
    if (!outVms) {                               // 出力先がなければ何もできない
        return VMResult::failure("内部エラー : 出力先が指定されていません");
    }
    *outVms = ::listVMs();                      // 既存関数の結果を出力引数へコピー
    return VMResult::success();                 // 旧関数は失敗を通知しないため常に成功扱い（改善は後続フェーズ）
}

VMResult LibvirtVMService::getVncConsoleInfo(const std::string& name, VncConsoleInfo* outInfo)
{
    if (!outInfo) {
        return VMResult::failure("内部エラー : 出力先が指定されていません");
    }
    std::string errorMessage;
    bool ok = ::getVncConsoleInfo(name, outInfo, &errorMessage);
    return toResult(ok, errorMessage, "VNC接続情報の取得に失敗しました");
}

// ---- スナップショット ----

VMResult LibvirtVMService::createSnapshot(
    const std::string& vmName,
    const std::string& snapshotName,
    const std::string& description)
{
    std::string errorMessage;
    bool ok = ::createSnapshot(vmName, snapshotName, description, &errorMessage);
    return toResult(ok, errorMessage, "スナップショットの作成に失敗しました");
}

VMResult LibvirtVMService::listSnapshots(const std::string& vmName, std::vector<SnapshotInfo>* outSnapshots)
{
    if (!outSnapshots) {
        return VMResult::failure("内部エラー : 出力先が指定されていません");
    }
    std::string errorMessage;
    *outSnapshots = ::listSnapshots(vmName, &errorMessage);
    if (!errorMessage.empty()) {               // 旧関数は「errorMessage が入る=失敗」という仕様
        return VMResult::failure(errorMessage);
    }
    return VMResult::success();
}

VMResult LibvirtVMService::revertSnapshot(const std::string& vmName, const std::string& snapshotName)
{
    std::string errorMessage;
    bool ok = ::revertSnapshot(vmName, snapshotName, &errorMessage);
    return toResult(ok, errorMessage, "スナップショットの復元に失敗しました");
}

VMResult LibvirtVMService::deleteSnapshot(const std::string& vmName, const std::string& snapshotName)
{
    std::string errorMessage;
    bool ok = ::deleteSnapshot(vmName, snapshotName, &errorMessage);
    return toResult(ok, errorMessage, "スナップショットの削除に失敗しました");
}

