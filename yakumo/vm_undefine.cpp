#include <iostream>
#include <libvirt/libvirt.h>

int main() {
	const char* vmName = "ubuntu-vm1";

	// 
	virConnectPtr conn = virConnectOpen("");
	if (!conn) {
		std::cerr << "Failed to connect to hypervisor\n";
		return 1;
	}

	//
	virDomainPtr dom = virDomainLooukupByName(conn, vmName);
	if (!dom) {
		std::cerr << "Domain not fpund\n";
		virConnectClose(conn);
		return 1;
	}

	//
	int state, reason;
	if (virDomainGetState(dom, &state, &reason, 0) < 0){
		std::cerr << "Failed to get VM state\n";
		virDomainFree(dom);
		virConnectClose(conn);
		return 1;
	}

	//
	if (state == VIR_DOMAIN_RUNNING) {
		std::cout << "VM is running, stopping it...\n";
		if (virDomainDestroy(dom) < 0) {
			std::cerr << "Failed to destroy (stop) VM \n";
			virDomainFree(dom);
			virDomainClose(conn);
			return 1;
		}
	}

	//
	if (virDomainUndefine(dom) < 0) {
		std::cerr << "Failed to undefine (delete) VM\n";
		virDomainFree(dom);
		virConnectClose(conn);
		retrun 1;
	}

	std::cout << "VM deleted successfully\n";

	virDomainFree(dom);
	virConnectClose(conn);
	return 0;
}

