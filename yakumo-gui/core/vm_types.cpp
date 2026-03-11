#include "vm_types.h"
#include <QString>

QString stateToString(VMState state)
{
    switch (state)
    {
    case VMState::Running:
        return "Running";
    case VMState::Shutoff:
        return "Shutoff";
    case VMState::Paused:
        return "Paused";
    default:
        return "Unknown";
    }
}
