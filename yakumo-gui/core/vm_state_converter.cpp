#include "vm_state_converter.h"

#include <libvirt/libvirt.h>

/* libvirt state -> VMState */
VMState convertState(int state)
{
    switch (state){
    case VIR_DOMAIN_RUNNING:
        return VMState::Running;
    case VIR_DOMAIN_SHUTOFF:
        return VMState::Shutoff;
    case VIR_DOMAIN_PAUSED:
        return VMState::Paused;
    case VIR_DOMAIN_SHUTDOWN:
        return VMState::Shutdown;
    case VIR_DOMAIN_CRASHED:
        return VMState::Crashed;
    default:
        return VMState::Unknown;
    }
}
