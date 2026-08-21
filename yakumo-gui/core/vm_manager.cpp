
#include "vm_manager.h"
#include "libvirt_connection.h"
#include "vm_state_converter.h"

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <iostream>
#include <QThread>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <cctype>
#include <QDomDocument>
#include <QString>
#include <cstdlib>

static constexpr const char* LIBVIRT_URI = "qemu:///system";
static constexpr unsigned int MIN_MEMORY_MB = 256;
static constexpr unsigned int MAX_MEMORY_MB = 32768;
static constexpr unsigned int MIN_VCPUS = 1;
static constexpr unsigned int MAX_VCPUS = 16;

// エラー文をセットする補助関数（定義はファイル後半にあるため）
static void setError(std::string* errorMessage, const std::string& message);

// ファイル先頭のマジックナンバーでqcow2形式かどうかを判定する
static bool isQcow2File(const std::string& path)
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


// VMの起動
bool startVM(const std::string& name)
{
    LibvirtConnection conn;
    if(!conn.isValid()) {
        std::cerr << "Failed to connect to hypervisor\n";
		return false;
	}

    virDomainPtr dom = virDomainLookupByName(conn.get(), name.c_str());
	if(!dom) {
		std::cerr << "Domain not found\n";
		return false;
	}

	int ret = virDomainCreate(dom);
	virDomainFree(dom);

	return ret == 0;
}


// VMの再起動
bool rebootVM(const std::string &name)
{
    LibvirtConnection conn;
    if(!conn.isValid()){
        std::cerr << "Failed to connect to hypervisor\n";
        return false;
    }

    virDomainPtr dom = virDomainLookupByName(conn.get(), name.c_str());
    if (!dom){
        std::cerr << "Domain not found\n";
        return false;
    }

    int ret = virDomainReboot(dom, 0);

    virDomainFree(dom);
    return ret == 0;
}


// OSに通常終了を依頼したようなVMシャットダウン
bool shutdownVM(const std::string& name)
{
    LibvirtConnection conn;
    if (!conn.isValid()){
        std::cerr << "Failed to connect to hypervisor\n";
        return false;
    }

    virDomainPtr dom = virDomainLookupByName(conn.get(), name.c_str());
    if(!dom) {
        std::cerr << "Domain not found\n";
        return false;
    }

    int ret = virDomainShutdown(dom);

    virDomainFree(dom);
    return ret == 0;
}


//　電源断のようなVM強制停止
bool forceStopVM(const std::string &name)
{
    LibvirtConnection conn;
    if (!conn.isValid()){
        std::cerr << "Failed to connect to hypervisor\n";
        return false;
    }

    virDomainPtr dom = virDomainLookupByName(conn.get(), name.c_str());
    if(!dom) {
        std::cerr << "Domain not found\n";
        return false;
    }

    int ret = virDomainDestroy(dom);

    virDomainFree(dom);
    return ret == 0;
}

// 停止中のVMだけを削除する（VM定義を外す）
bool deleteVM(const std::string &name, std::string* errorMessage)
{
    LibvirtConnection conn;
    if (!conn.isValid()){
        setError(errorMessage, "Failed to connect to hypervisor");
        return false;
    }

    virDomainPtr dom = virDomainLookupByName(conn.get(), name.c_str());
    if(!dom) {
        setError(errorMessage, "Domain not found");
        return false;
    }

    // 起動中はVMを削除しない
    if (virDomainIsActive(dom) == 1) {
        setError(errorMessage, "Cannot delete a running VM. Shut it down first.");
        virDomainFree(dom);
        return false;
    }

    // スナップショットが残っていると undefine が失敗するので、件数を数えて先に知らせる
    int snapshotCount = virDomainSnapshotNum(dom, 0);
    if (snapshotCount > 0) {
        std::ostringstream message;
        message << "Cannot delete: " << snapshotCount << " snapshot(s) still exist. Delete the snapshots first.";
        setError(errorMessage, message.str());
        virDomainFree(dom);
        return false;
    }

    int ret = virDomainUndefine(dom);
    if (ret != 0) {
        // 想定外の失敗はlibvirtが返したエラー文をそのまま伝える
        std::string reason = virGetLastErrorMessage();
        setError(errorMessage, "Failed to undefine domain: " + reason);
    }

    virDomainFree(dom);
    return ret == 0;
}

// VM名確認
static bool isValidVMName(const std::string& name)
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

// エラー文をセットするための補助関数
static void setError(std::string* errorMessage, const std::string& message)
{
    if(errorMessage) {
        *errorMessage = message;
    }

    std::cerr << message << "\n";
}

// VMコンソール関数
bool getVncConsoleInfo(const std::string& name, VncConsoleInfo* info, std::string* errorMessage = nullptr)
{
    if(!info){
        setError(errorMessage, "VNC console info output is null");
        return false;
    }

    LibvirtConnection conn;
    if(!conn.isValid()){
        setError(errorMessage, "Failed to connect to hypervisor");
        return false;
    }

    virDomainPtr dom = virDomainLookupByName(conn.get(), name.c_str());

    if(!dom){
        setError(errorMessage, "Domain not found");
        return false;
    }

    if(virDomainIsActive(dom) != 1){
        setError(errorMessage, "VM is not running");
        virDomainFree(dom);
        return false;
    }

    char* xml = virDomainGetXMLDesc(dom, 0);

    if(!xml){
        setError(errorMessage, "Failed to get domain XML");
        virDomainFree(dom);
        return false;
    }

    QDomDocument document;
    QString parseError;
    int parseLine = 0;
    int parseColumn = 0;

    bool parsed = document.setContent(QString::fromUtf8(xml), &parseError, &parseLine, &parseColumn);

    free(xml);
    virDomainFree(dom);

    if(!parsed){
        setError(errorMessage, "Failed to parse domain XML");
        return false;
    }

    QDomNodeList graphicsNodes = document.elementsByTagName("graphics");

    for (int i = 0; i < graphicsNodes.count(); ++i){
        QDomElement graphics = graphicsNodes.at(i).toElement();

        if (graphics.attribute("type") != "vnc"){
            continue;
        }

        bool ok = false;
        int port = graphics.attribute("port").toInt(&ok);

        if (!ok || port <= 0){
            setError(errorMessage, "VNC port is not assigned");
            return false;
        }

        QString host = graphics.attribute("listen");

        if(host.isEmpty()){
            host = "127.0.0.1";
        }

        info->host = host.toStdString();
        info->port = port;

        return true;
    }

    setError(errorMessage, "VNC graphics device was not found");
    return false;
}


// VMの作成
bool createVM(
    const std::string& name,
    unsigned int memoryMB,
    unsigned int vcpus,
    const std::string& diskPath,
    std::string* errorMessage
)
{
    if (name.empty()) {
        setError(errorMessage, "VM name is empty");
        return false;
    }

    if (!isValidVMName(name)) {
        setError(errorMessage, "VM name contains invalid characters");
        return false;
    }

    if (memoryMB < MIN_MEMORY_MB || memoryMB > MAX_MEMORY_MB) {
        std::ostringstream message;
        message << "Memory must be between "
                << MIN_MEMORY_MB << " and "
                << MAX_MEMORY_MB << " MB";

        setError(errorMessage, message.str());
        return false;
    }

    if (vcpus < MIN_VCPUS || vcpus > MAX_VCPUS) {
        std::ostringstream message;
        message << "vCPUs must be between "
                << MIN_VCPUS << " and "
                << MAX_VCPUS;

        setError(errorMessage, message.str());
        return false;
    }

    if (diskPath.empty()) {
        setError(errorMessage, "Disk path is empty");
        return false;
    }

    // VMイメージを取得するパスを絶対パスかどうかチェックする
    if (!std::filesystem::path(diskPath).is_absolute()) {
        setError(errorMessage, "Disk path must be absolute");
        return false;
    }

    if (!std::filesystem::exists(diskPath)) {
        setError(errorMessage, "Disk image file does not exists");
        return false;
    }

    if (!std::filesystem::is_regular_file(diskPath)) {
        setError(errorMessage, "Disk path is not a regular file");
        return false;
    }

    // 拡張子ではなくファイルの中身(マジックナンバー)で qcow2 かどうかを判定する
    if (!isQcow2File(diskPath)) {
        setError(errorMessage, "Disk image must be a qcow2 file");
        return false;
    }

    LibvirtConnection conn;
    if (!conn.isValid()) {
        setError(errorMessage, "Failed to connect to hypervisor");
        return false;
    }

    virDomainPtr existingDom = virDomainLookupByName(conn.get(), name.c_str());
    if (existingDom) {
        setError(errorMessage, "Domain already exists");
        virDomainFree(existingDom);
        return false;
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

    virDomainPtr dom = virDomainDefineXML(conn.get(), xml.str().c_str());
    if (!dom) {
        setError(errorMessage, "Failed to define domain");
        return false;
    }

    virDomainFree(dom);
    return true;
}
