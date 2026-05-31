
#include "vm_manager.h"
#include "libvirt_connection.h"
#include "vm_state_converter.h"

#include <libvirt/libvirt.h>
#include <iostream>
#include <QThread>
#include <sstream>
#include <filesystem>
#include <cctype>

static constexpr const char* LIBVIRT_URI = "qemu:///system";
static constexpr unsigned int MIN_MEMORY_MB = 256;
static constexpr unsigned int MAX_MEMORY_MB = 32768;
static constexpr unsigned int MIN_VCPUS = 1;
static constexpr unsigned int MAX_VCPUS = 16;

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
bool deleteVM(const std::string &name)
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

    // 起動中はVMを削除しない
    if (virDomainIsActive(dom) == 1) {
        std::cerr << "Cannot delete a running domain\n";
        virDomainFree(dom);
        return false;
    }

    int ret = virDomainUndefine(dom);

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

// VMの作成
bool createVM(
    const std::string& name,
    unsigned int memoryMB,
    unsigned int vcpus,
    const std::string& diskPath
)
{
    if (name.empty()) {
        std::cerr << "VM name is empty\n";
        return false;
    }

    if (!isValidVMName(name)) {
        std::cerr << "VM name contains invalid characters\n";
        return false;
    }

    if (memoryMB < MIN_MEMORY_MB || memoryMB > MAX_MEMORY_MB) {
        std::cerr << "Memory must be between "
                  << MIN_MEMORY_MB << " amd "
                  << MAX_MEMORY_MB << " MB\n";
        return false;
    }

    if (vcpus < MIN_VCPUS || vcpus > MAX_VCPUS) {
        std::cerr << "vCPUs must be between "
                  << MIN_VCPUS << " and "
                  << MAX_VCPUS << "\n";
        return false;
    }

    if (diskPath.empty()) {
        std::cerr << "Disk path is empty\n";
        return false;
    }

    // VMイメージを取得するパスを絶対パスかどうかチェックする
    if (!std::filesystem::path(diskPath).is_absolute()) {
        std::cerr << "Disk path must be absolute\n";
        return false;
    }

    if (!std::filesystem::exists(diskPath)) {
        std::cerr << "Disk image file does not exists\n";
        return false;
    }

    if (!std::filesystem::is_regular_file(diskPath)) {
        std::cerr << "Disk path is not a regular file\n";
        return false;
    }

    // qcow2のみを許可するためのチェック
    if (std::filesystem::path(diskPath).extension() != ".qcow2") {
        std::cerr << "Disk image must be a qcow2 file\n";
        return false;
    }

    LibvirtConnection conn;
    if (!conn.isValid()) {
        std::cerr << "Failed to connect to hypervisor\n";
        return false;
    }

    virDomainPtr existingDom = virDomainLookupByName(conn.get(), name.c_str());
    if (existingDom) {
        std::cerr << "Domain already exists\n";
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
        << "<console type='pty'/>"
        << "</devices>"
        << "</domain>";

    virDomainPtr dom = virDomainDefineXML(conn.get(), xml.str().c_str());
    if (!dom) {
        std::cerr << "Failed to define domain\n";
        return false;
    }

    virDomainFree(dom);
    return true;
}
