
#include "libvirt_vm_service.h"
#include "libvirt_connection.h"
#include "vm_state_converter.h"
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <sstream>
#include <QDomDocument>
#include <QString>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <cctype>



// このファイル内だけで使う補助関数
namespace {
// createVM の入力検証で使う上限・下限
constexpr unsigned int MIN_MEMORY_MB = 256;
constexpr unsigned int MAX_MEMORY_MB = 32768;
constexpr unsigned int MIN_VCPUS = 1;
constexpr unsigned int MAX_VCPUS = 16;

// ファイル先頭のマジックナンバーでqcow2形式かどうかを判定する
bool isQcow2File(const std::string& path)
{
    // バイナリモードでファイルを開く
    std::ifstream file(path, std::ios::binary);
    if (!file){
        return false;
    }

    // 先頭4バイトを読み込む
    unsigned char magic[4] = {0};
    file.read(reinterpret_cast<char*>(magic), 4);
    if (file.gcount() != 4){
        return false;
    }

    // qcow2 のマジックナンバーは "QFI\xFB" (0x51 0x46 0x49 0xFB)
    return magic[0] == 0x51 &&
           magic[1] == 0x46 &&
           magic[2] == 0x49 &&
           magic[3] == 0xFB;
}

// VM名・スナップショット名の文字チェック（英数字と - _ . のみ許可）
bool isValidName(const std::string& name)
{
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))){
            continue;
        }

        if (c == '-' || c == '_' || c == '.'){
            continue;
        }

        return false;
    }

    return true;
}

// XML文字確認
static std::string escapeXml(const std::string& text)
{
    std::string escaped;
    escaped.reserve(text.size());

    for (char c : text) {
        switch (c){
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '"':
            escaped += "&quot;";
            break;
        case '\'':
            escaped += "&apos;";
            break;
        default:
            escaped += c;
            break;
        }
    }
    return escaped;
}
} // namespace

// ---- VMライフサイクル ----

VMResult LibvirtVMService::createVM(
    const std::string& name,
    unsigned int memoryMB,
    unsigned int vcpus,
    const std::string& diskPath)
{
    if (name.empty()) {
        return VMResult::failure("VM name is empty");
    }

    if (!isValidName(name)) {
        return VMResult::failure("VM name contains invalid characters");
    }

    if (memoryMB < MIN_MEMORY_MB || memoryMB > MAX_MEMORY_MB) {
        std::ostringstream message;
        message << "Memory must be between " << MIN_MEMORY_MB << " and " << MAX_MEMORY_MB << " MB";
        return VMResult::failure(message.str());
    }

    if (vcpus < MIN_VCPUS || vcpus > MAX_VCPUS) {
        std::ostringstream message;
        message << "vCPUs must be between " << MIN_VCPUS << " and" << MAX_VCPUS;
        return VMResult::failure(message.str());
    }

    if (diskPath.empty()) {
        return VMResult::failure("Disk path is empty");
    }

    if (!std::filesystem::path(diskPath).is_absolute()) {
        return VMResult::failure("Disk path must be absolute");
    }

    if (!std::filesystem::path(diskPath).is_absolute()) {
        return VMResult::failure("Disk path must be absolute");
    }

    if (!std::filesystem::exists(diskPath)) {
        return VMResult::failure("Disk image file does not exist");
    }

    if (!std::filesystem::is_regular_file(diskPath)) {
        return VMResult::failure("Disk path is not a regular file");
    }

    // 拡張子ではなくファイルの中身（マジックナンバー）でqcow2かどうかを判定する
    if (!isQcow2File(diskPath)) {
        return VMResult::failure("Disk image must be a qcow2 file");
    }

    LibvirtConnection conn;
    if (!conn.isValid()) {
        return VMResult::failure("Failed to connect to hypervisor");
    }

    virDomainPtr existingDom = virDomainLookupByName(conn.get(), name.c_str());
    if (existingDom) {
        virDomainFree(existingDom);             // 見つかったハンドルを解放してから拒否
        return VMResult::failure("Domain already exists");
    }

    std::string escapedName = escapeXml(name);
    std::string escapedDiskPath = escapeXml(diskPath);

    std::ostringstream xml;

    xml
        << "<domain type='kvm'>"
        << "<name>" << escapedName << "</name>"
        << "<memory unit='MiB'>" << memoryMB << "</memory>"
        << "<vcpu>" << vcpus << "</vcpu>"
        << "<os>"
        << "<type arch='x86_64'>hvm</type>"
        << "</os>"
        << "<devices>"
        << "<disk type='file' device='disk'>"
        << "<driver name='qemu' type='qcow2'/>"
        << "<source file='" << escapedDiskPath << "'/>"
        << "<target dev='vda' bus='virtio'/>"
        << "</disk>"
        << "<interface type='network'>"
        << "<source network='default'/>"
        << "<model type='virtio'/>"
        << "</interface>"
        << "<graphics type='vnc' port='-1' autoport='yes' listen='127.0.0.1'>"
        << "<listen type='address' address='127.0.0.1'/>"
        << "</graphics>"
        << "<video>"
        << "<model type='virtio'/>"
        << "</video>"
        << "<input type='tablet' bus='usb'/>"
        << "<console type='pty'/>"
        << "</devices>"
        << "</domain>";

    virDomainPtr dom = virDomainDefineXML(conn.get(), xml.str().c_str());   // XMLをlibvirtに登録
    if (!dom) {
        return VMResult::failure("Failed to define domain");
    }

    virDomainFree(dom);
    return VMResult::success();
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
    if (snapshotName.empty()) {
        return VMResult::failure("Snapshot name is empty");
    }

    if (!isValidName(snapshotName)) {
        return VMResult::failure("Snapshot name contains invalid characters");
    }

    LibvirtConnection conn;
    if (!conn.isValid()) {
        return VMResult::failure("Failed to connect to hypervisor");
    }

    virDomainPtr dom = virDomainLookupByName(conn.get(), vmName.c_str());
    if (!dom) {
        return VMResult::failure("Domain not found: " + vmName);
    }

    // スナップショット定義XMLを組み立てる（名前と説明はエスケープ）
    std::string escapedName = escapeXml(snapshotName);
    std::string escapedDesc = escapeXml(description);

    std::ostringstream xml;
    xml << "<domainsnapshot>"
        << "<name>"         << escapedName << "</name>"
        << "<description>"  << escapedDesc << "</description>"
        << "</domainsnapshot>";

    // flags=0 → 稼働中ならRAM込み、停止中はディスクのみ
    virDomainSnapshotPtr snap = virDomainSnapshotCreateXML(dom, xml.str().c_str(), 0);

    if (!snap) {
        virDomainFree(dom);
        return VMResult::failure("Failed to create snapshot");
    }

    virDomainSnapshotFree(snap);        // 確保した順と逆に解放
    virDomainFree(dom);
    return VMResult::success();

}

VMResult LibvirtVMService::listSnapshots(const std::string& vmName, std::vector<SnapshotInfo>* outSnapshots)
{
    if (!outSnapshots) {
        return VMResult::failure("内部エラー : 出力先が指定されていません");
    }
    outSnapshots->clear();      // 前回の内容が残らないように空にしておく

    LibvirtConnection conn;
    if (!conn.isValid()) {
        return VMResult::failure("Failed to connect to hypervisor");
    }

    virDomainPtr dom = virDomainLookupByName(conn.get(), vmName.c_str());
    if (!dom) {
        return VMResult::failure("Domain not found: " + vmName);
    }

    virDomainSnapshotPtr* snaps = nullptr;      // スナップショットの配列（libvirtが確保する）
    int count = virDomainListAllSnapshots(dom, &snaps, 0);

    if (count < 0) {
        virDomainFree(dom);
        return VMResult::failure("Failed to list snapshots");
    }

    for (int i = 0; i < count; i++) {
        SnapshotInfo info{};                    // メンバを0/空で初期化

        char* xml = virDomainSnapshotGetXMLDesc(snaps[i], 0);
        if (xml) {
            QDomDocument doc;
            if (doc.setContent(QString::fromUtf8(xml))) {
                QDomElement root  = doc.documentElement();
                info.name         = root.firstChildElement("name").text().toStdString();
                info.description  = root.firstChildElement("description").text().toStdString();
                info.state        = root.firstChildElement("state").text().toStdString();
                info.parent       = root.firstChildElement("parent")
                                        .firstChildElement("name").text().toStdString();
                info.creationTime = root.firstChildElement("creationTime").text().toLongLong();
            }
            free(xml);             // libvirt が確保した文字列を解放
        }

        info.isCurrent = (virDomainSnapshotIsCurrent(snaps[i], 0) == 1);

        outSnapshots->push_back(info);      // 出力引数へ追加
        virDomainSnapshotFree(snaps[i]);
    }

    free(snaps);
    virDomainFree(dom);
    return VMResult::success();
}


VMResult LibvirtVMService::revertSnapshot(const std::string& vmName, const std::string& snapshotName)
{
    LibvirtConnection conn;
    if (!conn.isValid()) {
        return VMResult::failure("Failed to connect to hypervisor");
    }

    virDomainPtr dom = virDomainLookupByName(conn.get(), vmName.c_str());
    if (!dom) {
        return VMResult::failure("Domain not found: " + vmName);
    }

    virDomainSnapshotPtr snap = virDomainSnapshotLookupByName(dom, snapshotName.c_str(), 0);
    if (!snap) {
        virDomainFree(dom);
        return VMResult::failure("Snapshot not found: " + snapshotName);
    }

    int ret = virDomainRevertToSnapshot(snap, 0);       // 復元実行

    virDomainSnapshotFree(snap);
    virDomainFree(dom);

    return (ret == 0) ? VMResult::success()
                      : VMResult::failure("Failed to revert snapshot: " + snapshotName);
}

VMResult LibvirtVMService::deleteSnapshot(const std::string& vmName, const std::string& snapshotName)
{
    LibvirtConnection conn;
    if (!conn.isValid()) {
        return VMResult::failure("Failed to connect to hypervisor");
    }

    virDomainPtr dom = virDomainLookupByName(conn.get(), vmName.c_str());
    if (!dom) {
        return VMResult::failure("Domain not found: " + vmName);
    }

    virDomainSnapshotPtr snap = virDomainSnapshotLookupByName(dom, snapshotName.c_str(), 0);
    if (!snap) {
        virDomainFree(dom);
        return VMResult::failure("Snapshot not found: " + snapshotName);
    }

    // flags=0 : このスナップショットのみ削除（子は親側へ付け替えられる）
    int ret = virDomainSnapshotDelete(snap, 0);

    virDomainSnapshotFree(snap);
    virDomainFree(dom);

    return (ret == 0) ? VMResult::success()
                      : VMResult::failure("Failed to delete snapshot: " + snapshotName);
}

