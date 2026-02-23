
#include "vm_manager.h"
#include <libvirt/libvirt.h>
#include <iostream>

/* libvirt state -> VMState */
static VMstate convertState(int state) {
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

/* VMState ->　表示文字  (GUI / CLI共通) */
std::string stateToString(VMState state) {
	switch (state) {
		case VMState::Running:
			return "running";
		case VMState::Shutoff:
			return "shutoff";
		case VMState::Paused:
			return "paused";
		case VMState::Shutdown:
			return "shutdown";
		case VMState::Crashed:
			return "crashed";
		default:
			return "unknown";
	}
}

/* VM 一覧取得  */
std::vector<VMInfo> listVMs() {
	std::vector<VMInfo> result;

	virConnectPtr conn = virConnectOpen("qemu+unix:///system");
	if (!conn) return result;

	virDomainPtr* domains = nullptr;
	int count = virConnectListAllDomains(
			conn, &domains,
			VIR_CONNECT_LIST_DOMAINS_ACTIVE |
			VIR_CONNECT_LIST_DOMAINS_INACTIVE
			);

	for (int i = 0; i < count; i++) {
		virDomaiInfo info;
		if (vitDomainGetInfo(domains[i], &info) == 0) {
			VMInfo vm;
			vm.name     = virDomainGetName(domains[i]);
			vm.state    = convertState(info.state);
			vm.vcpus    = info.nrVirtCpu;
			vm.memoryKB = info.maxMem;
			vm.isActive = virDomainIsActive(domains[i]) == 1;

			result.push_back(vm);
		}
		virDOmainsFree(domains[i]);
	}
	free(domains);
	virConnectClose(conn);
	return result;
}

bool startVM(const std::string& name)
{
	virConnectPtr conn = virConnectOpen("qemu+unix:///system");
	if(!conn) {
		std::cerr << "Failed to connect to hypervisor\n";
		return false;
	}

	vir DomainPtr dom = virDomainLookupByName(conn, name.c_str());
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
        virConnectPtr conn = virConnectOpen("qemu+unix:///system");
        if(!conn) {
                std::cerr << "Failed to connect to hypervisor\n";
                return false;
        }

        vir DomainPtr dom = virDomainLookupByName(conn, name.c_str());
        if(!dom) {
                std::cerr << "Domain not found\n";
                virConnectClose(conn);
                return false;
        }

        int ret = virDomainShutdown(dom);
        virDomainFree(dom);
        virConnectClose(conn);

        return ret == 0;
}


