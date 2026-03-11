#include <iostream>
#include <unistd.h>
#include <libvirt/libvirt.h>

int main() {
	const char * vmName = "ubuntu-vm1";

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

	if (virDomainShutdown(dom) < 0) {
		std::cerr << "Failed to shutdown domain\n";
	} else {
		std::cout << "Shutdown signal sent, waiting for domain to stop..." << std::endl;

		int state;
		while (true) {
			if (virDomainGetState(dom, &state, NULL, 0) < 0) {
				std::cerr << "Failed to get domain state\n";
				break;
			}
			if (state == VIR_DOMAIN_SHUTOFF) {
				std::cout << "Domain has completery shutdown\n";
				break;
			}

			sleep(1); //1秒待機して再確認
		}
	}

	virDomainFree(dom);
	virConnectClose(conn);
	return 0;
}
