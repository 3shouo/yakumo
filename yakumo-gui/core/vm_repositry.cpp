
#include "vm_repositry.h"
#include "libvirt_connection.h"
#include "vm_state_converter.h"

#include <libvirt/libvirt.h>

std::vector<VMInfo> listVMs()
{
    std::vector<VMInfo> result;

    LibvirtConnection conn;
    if (!conn.isValid())
    {
        return result;
    }

    virDomainPtr* domains = nullptr;

    int count = virConnectListAllDomains(
        conn.get(),
        &domains,
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

    return result;
}

