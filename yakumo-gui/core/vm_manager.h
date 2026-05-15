
#pragma once
#include <vector>
#include <string>
#include "vm_types.h"

bool createVM(
    const std::string& name,
    unsigned int memoryMB,
    unsigned int vcpus,
    const std::string& diskPath
);
bool startVM(const std::string& name);
bool shutdownVM(const std::string& name);
bool forceStopVM(const std::string& name);
bool rebootVM(const std::string& name);
bool deleteVM(const std::string& name);
std::vector<VMInfo> listVMs();

