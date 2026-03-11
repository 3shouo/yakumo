
#include "vm_manager.h"
#include <libvirt/libvirt.h>
#include <iostream>
#include <QThread>

/* libvirt state -> VMState */
static VMState convertState(int state) {
	switch (state){
		case VIR_DOMAIN_RUNNING:
			return VMState::Running;
		case VIR_DOMAIN_SHUTOFF:
			return VMState::Shutoff;
		case VIR_DOMAIN_PAUSED:
			return VMState::Paused;
		case VIR_DOMAIN_SHUTDOWN:
			return VMState::Shutdown;
		case VIR_DOMAIN_CRASHED:
			return VMState::Crashed;
		default:
			return VMState::Unknown;
	}
}


std::vector<VMInfo> listVMs() {
	std::vector<VMInfo> result;

    virConnectPtr conn = virConnectOpen("qemu:///system");
	if (!conn) return result;

	virDomainPtr* domains = nullptr;
	int count = virConnectListAllDomains(
			conn, &domains,
			VIR_CONNECT_LIST_DOMAINS_ACTIVE |
			VIR_CONNECT_LIST_DOMAINS_INACTIVE
			);

	for (int i = 0; i < count; i++) {
        virDomainInfo info;
        if (virDomainGetInfo(domains[i], &info) == 0) {
			VMInfo vm;
			vm.name     = virDomainGetName(domains[i]);
			vm.state    = convertState(info.state);
			vm.vcpus    = info.nrVirtCpu;
            vm.memoryMB = info.maxMem;
			vm.isActive = virDomainIsActive(domains[i]) == 1;

			result.push_back(vm);
		}
        virDomainFree(domains[i]);
	}
	free(domains);
	virConnectClose(conn);
	return result;
}


bool startVM(const std::string& name)
{
    virConnectPtr conn = virConnectOpen("qemu:///system");
	if(!conn) {
		std::cerr << "Failed to connect to hypervisor\n";
		return false;
	}

    virDomainPtr dom = virDomainLookupByName(conn, name.c_str());
	if(!dom) {
		std::cerr << "Domain not found\n";
		virConnectClose(conn);
		return false;
	}

	int ret = virDomainCreate(dom);
	virDomainFree(dom);
	virConnectClose(conn);

	return ret == 0;
}

bool shutdownVM(const std::string& name)
{
    virConnectPtr conn = virConnectOpen("qemu:///system");
	if (!conn)
		return false;

	virDomainPtr dom = virDomainLookupByName(conn, name.c_str());
	if(!dom) {
                std::cerr << "Domain not found\n";
                virConnectClose(conn);
                return false;
        }

	//まずは通常シャットダウン
	if (virDomainShutdown(dom) < 0)
	{
		virDomainFree(dom);
		virConnectClose(conn);
		return false;
	}

	//最大10秒待つ
	for (int i = 0; i < 10; ++i)
	{
		int state;
		int reason;
		virDomainGetState(dom, &state, &reason, 0);

		if (state == VIR_DOMAIN_SHUTOFF)
		{
			break;
		}

		QThread::sleep(1);
	}

	//まだRunningなら強制停止
	int state;
	int reason;
	virDomainGetState(dom, &state, &reason, 0);

	if (state != VIR_DOMAIN_SHUTOFF)
	{
		virDomainDestroy(dom);
	}

	virDomainFree(dom);
	virConnectClose(conn);

	return true;
}

