#include <iostream>
#include <libvirt/libvirt.h>

int main() {
	const char* vmName = "ubuntu-vm1";

	virConnectPtr conn = virConnectOpen("qemu+unix:///system");
	if(!conn) {
		std::cerr << "Failed to Connect to hypervisor\n";
		return 1;
	}

	virDomainPtr dom = virDomainLookupByName(conn, vmName);
	if(!dom) {
		std::cerr << "Domain not found\n";
		virConnectClose(conn);
		return 1;
	}

	if (virDomainCreate(dom) < 0) {
		std::cerr << "Failed to start domain\n";
	} else {
		std::cout << "Domain started successfully\n";
	}

	virDomainFree(dom);
	virConnectClose(conn);
	return 0;
}
