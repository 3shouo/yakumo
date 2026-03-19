
#include "vm_manager.h"
#include "libvirt_connection.h"
#include "vm_state_converter.h"

#include <libvirt/libvirt.h>
#include <iostream>
#include <QThread>

static constexpr const char* LIBVIRT_URI = "qemu:///system";


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
        std::cerr << "Domain not fpund\n";
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

