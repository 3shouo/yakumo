
#pragma once

#include <libvirt/libvirt.h>

class LibvirtConnection
{
public:
    LibvirtConnection();
    ~LibvirtConnection();

    virConnectPtr get() const;
    bool isValid() const;

private:
    virConnectPtr conn_;
};
