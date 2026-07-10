
#pragma once
#include <QString>

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

QString stateToString(VMState state);

/* スナップショット情報 */
struct SnapshotInfo {
	std::string name;			// スナップショット名
	std::string description;	// 説明（無ければ空文字）
	std::string parent;			// 親スナップショット名（無ければ空文字）
	std::string state;			// 作成時のVM状態（"running" / "shutoff" など）
	long long 	creationTime;		// 作成時刻（UNIXエポック秒）
	bool      	isCurrent;		// current（現在地）かどうか
};

