
#include <iostream>
#include <libvirt/libvirt.h>

int main() {
    virConnectPtr conn = virConnectOpen("qemu:///system");
	if (!conn){
		std::cerr << "Failed to connect to hypervisor\n";
		return 1;
	}

	const char* domainXML = R"(
<domain type='kvm'>
        <name>ubuntu-vm2</name>

        <memory unit='MiB'>1536</memory>
        <vcpu>1</vcpu>

        <os>
                <type arch='aarch64'>hvm</type>
        </os>

        <devices>
                <disk type='file' device='disk'>
                        <driver name='qemu' type='qcow2'/>
                        <source file='/var/lib/libvirt/images/ubuntu-24.04.qcow2'/>
                        <target dev='vda' bus='virtio'/>
                </disk>

                <interface type='network'>
                        <source network='default'/>
                        <model type='virtio'/>
                </interface>

                <console type='pty'/>
        </devices>
</domain>
)";

	virDomainPtr dom = virDomainDefineXML(conn, domainXML);
	if (!dom) {
		std::cerr << "Failed to define domain\n";
		virConnectClose(conn);
		return 1;
	}

	std::cout << "Domain defined successfully\n";

	virDomainFree(dom);
	virConnectClose(conn);
	return 0;
}
