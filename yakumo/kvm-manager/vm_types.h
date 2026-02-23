
#pragma once
#include <string>

/* VMの状態(libvirt 非依存) */
enums class VMstate {
	Running,
	Shutoff,
	Paused,
	Shutdown,
	Crashed,
	Unknown
};

/* VM情報*/
struct VMinfo {
	std::string name;          //VM情報
	VMstate state;             //状態
	unsigned int vcpus;        //vCPU数
	unsigned long memoryKB;    //メモリ(KB)
	bool isActive;             //稼働中かどうか
};

