
#include "vm_manager.h"
#include <iostream>

int main() {
	auto vms = listVMs();
	for (const auto& vm : vms) {
		std::cout << vm.name << std::endl;
	}
}
