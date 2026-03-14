#include "libvirt_connection.h"

static constexpr const char* LIBVIRT_URI = "qemu:///system";

LibvirtConnection::LibvirtConnection()
    : conn_(virConnectOpen(LIBVIRT_URI))
{

}


LibvirtConnection::~LibvirtConnection()
{
    if(conn_){
        virConnectClose(conn_);
    }
}

virConnectPtr LibvirtConnection::get() const
{
    return conn_;
}

bool LibvirtConnection::isValid() const
{
    return conn_ != nullptr;
}
