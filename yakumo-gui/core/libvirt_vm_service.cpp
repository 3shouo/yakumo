
#include "libvirt_vm_service.h"
#include "libvirt_connection.h"
#include "vm_state_converter.h"
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <sstream>
#include <QDomDocument>
#include <QString>
#include <cstdlib>

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
    LibvirtConnection conn;         // 接続を開く（関数を抜けるときに自動で閉じる）
    if(!conn.isValid()){
        return VMResult::failure("Failed to connect to hypervisor");
    }

    virDomainPtr dom = virDomainLookupByName(conn.get(), name.c_str());     //名前からVM（ドメイン）を検索
    if(!dom){
        return VMResult::failure("Domain not found: " + name);
    }

    int ret = virDomainCreate(dom);         // 起動（libvirtではCreateが「起動」の意味）
    virDomainFree(dom);                     // ドメインハンドルを開放

    return (ret == 0) ? VMResult::success()
                      : VMResult::failure("Failed to start VM: " + name);
}

VMResult LibvirtVMService::shutdownVM(const std::string& name)
{
    LibvirtConnection conn;
    if(!conn.isValid()){
        return VMResult::failure("Failed to connect to hypervisor");
    }

    virDomainPtr dom = virDomainLookupByName(conn.get(), name.c_str());
    if(!dom){
        return VMResult::failure("Domain not found: " + name);
    }

    int ret = virDomainShutdown(dom);      // ゲストOSへ通常終了を依頼
    virDomainFree(dom);

    return (ret == 0) ? VMResult::success()
                      : VMResult::failure("Failed to shutdown VM: " + name);
}

VMResult LibvirtVMService::forceStopVM(const std::string& name)
{
    LibvirtConnection conn;
	if (!conn.isValid()) {
		return VMResult::failure("Failed to connect to hypervisor");
	}

	virDomainPtr dom = virDomainLookupByName(conn.get(), name.c_str());
	if (!dom) {
		return VMResult::failure("Domain not found: " + name);
	}

    int ret = virDomainDestroy(dom);    // 電源断相当の強制停止
    virDomainFree(dom);

    return (ret == 0) ? VMResult::success()
	                  : VMResult::failure("Failed to force-stop VM: " + name);
}

VMResult LibvirtVMService::rebootVM(const std::string& name)
{
    LibvirtConnection conn;
	if (!conn.isValid()) {
		return VMResult::failure("Failed to connect to hypervisor");
	}

	virDomainPtr dom = virDomainLookupByName(conn.get(), name.c_str());
	if (!dom) {
		return VMResult::failure("Domain not found: " + name);
	}

    int ret = virDomainReboot(dom, 0);  // 第2引数 0 = デフォルト方式で再起動
    virDomainFree(dom);

    return (ret == 0) ? VMResult::success()
	                  : VMResult::failure("Failed to reboot VM: " + name);
}

VMResult LibvirtVMService::deleteVM(const std::string& name)
{
    LibvirtConnection conn;
    if (!conn.isValid()) {
        return VMResult::failure("Failed to connect to hypervisor");
    }

    virDomainPtr dom = virDomainLookupByName(conn.get(), name.c_str());
    if (!dom) {
        return VMResult::failure("Domain not found: " + name);
    }

    // 起動中はVMを削除しない
    if (virDomainIsActive(dom) == 1) {
        virDomainFree(dom);             // return する前にハンドルを開放
        return VMResult::failure("Cannot delete a running VM. Shut it down first.");
    }

    // スナップショットが残っていると undefine が失敗するので、件数を数えて先に知らせる
    int snapshotCount = virDomainSnapshotNum(dom, 0);
    if (snapshotCount > 0) {
        virDomainFree(dom);
        std::ostringstream message;    // 数値を文中に埋め込むための文字列ストリーム
        message << "Cannot delete: " << snapshotCount << " snapshot(s) still exist. Delete the snapshots first.";
        return VMResult::failure(message.str());
    }

    int ret = virDomainUndefine(dom);   // VM定義の削除（ディスクイメージを消さない）
    virDomainFree(dom);

    if (ret != 0) {
        // 想定外の失敗は libvirt が返したエラー文をそのまま伝える
        std::string reason = virGetLastErrorMessage();
        return VMResult::failure("Failed to undefined domain: " + reason);
    }
    return VMResult::success();
}

// ---- 情報取得 ----

VMResult LibvirtVMService::listVMs(std::vector<VMInfo>* outVms)
{
    if (!outVms) {                              // 出力先がなければ何もできない
        return VMResult::failure("内部エラー : 出力先が指定されていません");
    }
    outVms->clear();                            // 前回の内容が残らないように空にしておく

    LibvirtConnection conn;
    if (!conn.isValid()) {
        return VMResult::failure("Failed to connect to hypervisor");
    }

    virDomainPtr* domains = nullptr;            // ドメインの配列（libvirt側が確保している）
    int count = virConnectListAllDomains(
        conn.get(),
        &domains,
        VIR_CONNECT_LIST_DOMAINS_ACTIVE |       // 稼働中と
        VIR_CONNECT_LIST_DOMAINS_INACTIVE       // 停止中の両方を列挙
    );

    if (count < 0) {                            // 列挙自体の失敗（-1が返る）
        std::string reason = virGetLastErrorMessage();
        return VMResult::failure("Failed to list domains: " + reason);
    }

    for (int i = 0; i < count; i++) {
        virDomainInfo info;
        if (virDomainGetInfo(domains[i], &info) == 0) {
            VMInfo vm;
            vm.name     = virDomainGetName(domains[i]);
            vm.state    = convertState(info.state);
            vm.vcpus    = info.nrVirtCpu;
            vm.memoryMB = info.maxMem / 1024;
            vm.isActive = virDomainIsActive(domains[i]) == 1;
            outVms->push_back(vm);
        }
        virDomainFree(domains[i]);              // 個々のドメインハンドルを開放
    }
    free(domains);                              // 配列本体はlibvirtがCのmallocで確保したのでfreeで開放

    return VMResult::success();
}


VMResult LibvirtVMService::getVncConsoleInfo(const std::string& name, VncConsoleInfo* outInfo)
{
    if (!outInfo) {
        return VMResult::failure("内部エラー : 出力先が指定されていません");
    }
    //std::string errorMessage;
    //bool ok = ::getVncConsoleInfo(name, outInfo, &errorMessage);
    //return toResult(ok, errorMessage, "VNC接続情報の取得に失敗しました");

    LibvirtConnection conn;
    if (!conn.isValid()) {
        return VMResult::failure("Failed to connect to hypervisor");
    }

    virDomainPtr dom = virDomainLookupByName(conn.get(), name.c_str());
    if (!dom) {
        return VMResult::failure("Domain not found: " + name);
    }

    if (virDomainIsActive(dom) != 1) {              // 停止中のVMにVNC画面はない
        virDomainFree(dom);
        return VMResult::failure("VM is not running");
    }

    char* xml = virDomainGetXMLDesc(dom, 0);  // VM定義XMLをC文字列で取得
    if (!xml) {
        virDomainFree(dom);
        return VMResult::failure("Failed to get domain XML");
    }

    QDomDocument document;
    bool parsed = document.setContent(QString::fromUtf8(xml));              // XML文字列を解析してDOMツリー化

    free(xml);                                                              // libvirtがmallocで確保した文字列なのでfreeで解放
    virDomainFree(dom);                                                     // 以降はXMLだけで用が足りるのでここで解放

    if (!parsed) {
        return VMResult::failure("Failed to parse domain XML");
    }

    QDomNodeList graphicsNodes = document.elementsByTagName("graphics");    // <graphics>要素を全部集める

    for (int i = 0; i < graphicsNodes.count(); ++i) {
        QDomElement graphics = graphicsNodes.at(i).toElement();

        if (graphics.attribute("type") != "vnc") {                         // vnc以外（spiceなど）は読み飛ばす
            continue;
        }

        bool ok = false;
        int port = graphics.attribute("port").toInt(&ok);                   // port属性を数値へ変換（成否がokに入る）

        if (!ok || port <= 0) {                                             // 未割り当て時はport="-1"が入っている
            return VMResult::failure("VNC port is not assigned");
        }

        QString host = graphics.attribute("listen");
        if (host.isEmpty()) {
            host = "127.0.0.1";                                             // listen指定がなければローカルとみなす
        }

        outInfo->host = host.toStdString();                                 // 見つかった接続情報を出力引数へ書き込む
        outInfo->port = port;

        return VMResult::success();
    }

    return VMResult::failure("VNC graphics device was not found");
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

