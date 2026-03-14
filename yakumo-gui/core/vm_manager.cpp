
#include "vm_manager.h"
#include "libvirt_connection.h"
#include "vm_state_converter.h"

#include <libvirt/libvirt.h>
#include <iostream>
#include <QThread>

static constexpr const char* LIBVIRT_URI = "qemu:///system";

bool startVM(const std::string& name)
{
    LibvirtConnection conn;
    if(!conn.isValid()) {
        std::cerr << "Failed to connect to hypervisor\n";
		return false;
	}

    virDomainPtr dom = virDomainLookupByName(conn.get(), name.c_str());
	if(!dom) {
		std::cerr << "Domain not found\n";
		return false;
	}

	int ret = virDomainCreate(dom);
	virDomainFree(dom);

	return ret == 0;
}

bool shutdownVM(const std::string& name)
{
    LibvirtConnection conn;
    if (!conn.isValid())
		return false;

    virDomainPtr dom = virDomainLookupByName(conn.get(), name.c_str());
	if(!dom) {
                std::cerr << "Domain not found\n";

                return false;
        }

	//まずは通常シャットダウン
	if (virDomainShutdown(dom) < 0)
	{
		virDomainFree(dom);

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

	return true;
}

