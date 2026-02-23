
#pragma once
#include <vector>
#include <string>
#include "vm_types.h"

bool startVM(const std::string& name);
bool shutdownVM(const std::string& name);
bool deleteVM(const std::string& name);
std::vector<VMInfo> listVMs();

