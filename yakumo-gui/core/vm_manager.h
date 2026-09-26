
#pragma once
#include <string>
#include "vm_types.h"

/*
struct VncConsoleInfo {
    std::string host;
    int port;
};
*/

bool createVM(
    const std::string& name,
    unsigned int memoryMB,
    unsigned int vcpus,
    const std::string& diskPath,
    std::string* errorMessage = nullptr
);
