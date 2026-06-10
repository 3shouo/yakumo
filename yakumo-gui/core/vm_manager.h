
#pragma once
#include <vector>
#include <string>
#include "vm_types.h"

struct VncConsoleInfo {
    std::string host;
    int port;
};

bool createVM(
    const std::string& name,
    unsigned int memoryMB,
    unsigned int vcpus,
    const std::string& diskPath,
    std::string* errorMessage = nullptr
);
bool startVM(const std::string& name);
bool shutdownVM(const std::string& name);
bool forceStopVM(const std::string& name);
bool rebootVM(const std::string& name);
bool deleteVM(const std::string& name);
bool getVncConsoleInfo(const std::string& name, VncConsoleInfo* info, std::string* errorMessage);

std::vector<VMInfo> listVMs();