
#pragma once
#include <vector>
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
bool deleteVM(const std::string& name, std::string* errorMessage = nullptr);
bool getVncConsoleInfo(const std::string& name, VncConsoleInfo* info, std::string* errorMessage);

std::vector<VMInfo> listVMs();