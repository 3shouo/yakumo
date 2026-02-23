#include <iostream>
#include <libvirt/libvirt.h>

int main() {
	const char* vmName = "ubuntu-vm1";

	virConnectPtr conn = virConnectOpen("qemu+unix:///system");
	if (!conn) {
		std::cerr << "Failed to Connect to hypervisor\n";
		return 1;
	}

	virDomainPtr dom = virDomainLookupByName(conn, vmName);
	if (!dom) {
		std::cerr << "Domain not found\n";
		virConnectClose(conn);
		return 1;
	}

	//VMの情報を取得
	virDomainInfo info;
	if (virDomainGetInfo(dom, &info) < 0) {
		std::cerr << "Failed to get domain info\n";
		virDomainFree(dom);
		virConnectClose(conn);
		return 1;
	}

	//stateを判定
	if (info.state == VIR_DOMAIN_RUNNING) {
		std::cout << "VM is running now\n";
	} else if (info.state == VIR_DOMAIN_SHUTOFF) {
		std::cout << "VM is shutdown\n";
	} else {
		std::cout << "VM is in state: " << info.state << "\n";
	}

	//後処理
	virDomainFree(dom);
	virConnectClose(conn);
	return 0;
}

