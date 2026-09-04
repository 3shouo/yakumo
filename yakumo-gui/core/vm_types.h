
#pragma once
#include <QString>
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

/* 操作結果（成功可否とエラーメッセージ） */
struct VMResult {
	bool ok = false;			// 成功したらtrue（初期値はfalse）
	std::string message;		// 失敗時のエラーメッセージ（成功時は空文字）

	// 成功結果を作るヘルパー
	static VMResult success() { return {true, ""};}
	// 失敗結果を作るヘルパー
	static VMResult failure(const std::string& msg) { return {false, msg}; }
};

/* VNC接続情報 */
struct VncConsoleInfo {
	std::string host;			// 接続先ホスト（例 "127.0.0.1"）
	int port = -1;				// VNCポート番号（未取得なら-1）
};


