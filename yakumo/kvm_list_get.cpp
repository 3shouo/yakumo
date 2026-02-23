#include <iostream>
#include <libvirt/libvirt.h>

int main(){
	virConnectPtr conn = virConnectOpen("qemu+unix:///system");
	if (conn == NULL) {
		std::cerr << "Failed to connect to hypervisor" << std::endl;
		return 1;
	}

	int numDomains = virConnectNumOfDomains(conn);
	int *activeDomains = new int[numDomains];
	numDomains = virConnectListDomains(conn, activeDomains, numDomains);

	std::cout << "Active VMs:" << std::endl;
	for (int i = 0; i < numDomains; i++) {
		std::cout << "VM ID:" << activeDomains[i] << std::endl;
	}

	delete[] activeDomains;
	virConnectClose(conn);
	return 0;
	}

