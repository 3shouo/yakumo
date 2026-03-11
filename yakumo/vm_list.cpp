#include <iostream>
#include <libvirt/libvirt.h>

int main() {
	//ハイパーバイザーに接続
	virConnectPtr conn = virConnectOpen("qemu+unix:///system");
	if(!conn){
		std::cerr << "Failed to connect to hypervisorn";
		return 1;
	}

	//すべてのドメイン（VM）を取得
	virDomainPtr *domains = nullptr;
	int numDomains = virConnectListAllDomains(
			conn,
			&domains,
			VIR_CONNECT_LIST_DOMAINS_ACTIVE | VIR_CONNECT_LIST_DOMAINS_INACTIVE
			);

	if(numDomains < 0) {
		std::cerr << "Failed to list domains\n";
		virConnectClose(conn);
		return 1;
	}

	std::cout << "=== VM List (Total: " << numDomains << ") ===\n";

	// 各VMの情報を表示
	for (int i = 0; i < numDomains; i++) {
		const char* name = virDomainGetName(domains[i]);
		virDomainInfo info;

		if(virDomainGetInfo(domains[i], &info) == 0) {
			std::cout << "VM Name: " << name;

			//状態の文字列変換
			std::string state;
			switch (info.state) {
				case VIR_DOMAIN_RUNNING:    state = "RUNNING"; break;
				case VIR_DOMAIN_BLOCKED:    state = "BLOCKED"; break;
				case VIR_DOMAIN_PAUSED:     state = "PAUSED"; break;
				case VIR_DOMAIN_SHUTDOWN:   state = "SHUTDOWN"; break;
				case VIR_DOMAIN_SHUTOFF:    state = "SHUTOFF"; break;
				case VIR_DOMAIN_CRASHED:    state = "CRASHED"; break;
				default:                    state = "UNKNOWN"; break;
			}

			std::cout << "  |  State: " << state << "\n";
		} else {
			std::cerr << "Failed to get info for domain: " << name << "\n";
		}
	}

	//メモリ解放
	for (int i = 0; i < numDomains; i++) {
		virDomainFree(domains[i]);
	}
	free(domains);

	virConnectClose(conn);
	return 0;
}
