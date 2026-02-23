
#pragma once
#include <string>

/* VMの状態(libvirt 非依存) */
enum class VMState {
	Running,
	Shutoff,
	Paused,
	Shutdown,
	Crashed,
	Unknown
};

/* VM情報*/
struct VMInfo {
	std::string name;          //VM情報
    VMState state;             //状態
	unsigned int vcpus;        //vCPU数
    unsigned long memoryMB;    //メモリ(MB)
	bool isActive;             //稼働中かどうか
};

