

#include <libvirt/libvirt.h>
#include <iostream>
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <QDomDocument>
#include <QString>

#include "snapshot_manager.h"
#include "libvirt_connection.h"

// エラー文をセットする補助関数
static void setError(std::string* errorMessage, const std::string& message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
    std::cerr << message << "\n";
}


// XML特殊文字をエスケープする
static std::string escapeXml(const std::string& text)
{
    std::string escaped;
    escaped.reserve(text.size());

    for (char c : text) {
        switch (c)
        {
        case '&':  escaped += "&amp;";  break;
        case '<':  escaped += "&lt;";   break;
        case '>':  escaped += "&gt;";   break;
        case '"':  escaped += "&quot;"; break;
        case '\'': escaped += "&apos;"; break;
        default:   escaped += c;        break;
        }
    }
    return escaped;
}

// スナップショット名の文字チェック
static bool isValidSnapshotName(const std::string& name)
{
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) continue;
        if (c == '-' || c == '_' || c == '.')            continue;
        return false;
    }
    return true;
}


// スナップショット作成関数
bool createSnapshot(
    const std::string& vmName,
    const std::string& snapshotName,
    const std::string& description,
    std::string* errorMessage
)
{
    // 入力チェック
    if (snapshotName.empty()) {
        setError(errorMessage, "Snapshot name is empty");
        return false;
    }

    if (!isValidSnapshotName(snapshotName)) {
        setError(errorMessage, "Snapshot name contains invalid characters");
        return false;
    }

    // ハイパーバイザへ接続
    LibvirtConnection conn;
    if (!conn.isValid()) {
        setError(errorMessage, "Failed to connect to hypervisor");
        return false;
    }

    // VM対象（ドメイン）を名前で取得
    virDomainPtr dom = virDomainLookupByName(conn.get(), vmName.c_str());
    if (!dom) {
        setError(errorMessage, "Domain not found");
        return false;
    }

    // スナップショット定義XMLを組み立てる
    std::string escapedName = escapeXml(snapshotName);
    std::string escapedDesc = escapeXml(description);

    std::ostringstream xml;
    xml << "<domainsnapshot>"
        << "<name>"         << escapedName << "</name>"
        << "<description>"  << escapedDesc << "</description>"
        << "</domainsnapshot>";
    
    // スナップショット作成（flags=0 → 稼働中ならRAM込み、停止中はディスクのみ）
    virDomainSnapshotPtr snap = virDomainSnapshotCreateXML(dom, xml.str().c_str(), 0);

    if (!snap) {
        setError(errorMessage, "Failed to create snapshot");
        virDomainFree(dom);
        return false;
    }

    // 後始末（確保した順と逆に解放）
    virDomainSnapshotFree(snap);
    virDomainFree(dom);
    return true;
}

// スナップショットリスト作成関数
std::vector<SnapshotInfo> listSnapshots(
    const std::string& vmName,
    std::string* errorMessage
)
{
    std::vector<SnapshotInfo> result;   // 返却用の空配列

    LibvirtConnection conn;
    if (!conn.isValid()) {
        setError(errorMessage, "Failed to connect to hypervisor");
        return result;                  // 空のまま返す
    }

    virDomainPtr dom = virDomainLookupByName(conn.get(), vmName.c_str());
    if (!dom) {
        setError(errorMessage, "Domain not found");
        return result;
    }

    // 全スナップショットを配列で取得（snaps は libvirt が確保する）
    virDomainSnapshotPtr* snaps = nullptr;
    int count = virDomainListAllSnapshots(dom, &snaps, 0);

    if (count < 0) {
        setError(errorMessage, "Failed to list snapshots");
        virDomainFree(dom);
        return result;
    }

    for (int i = 0; i < count; i++) {
        SnapshotInfo info{};        // メンバを0/空で初期化

        // 各スナップショットのXMLを取得してパース
        char* xml = virDomainSnapshotGetXMLDesc(snaps[i], 0);
        if (xml) {
            QDomDocument doc;
            if (doc.setContent(QString::fromUtf8(xml))) {
                QDomElement root  = doc.documentElement();
                info.name         = root.firstChildElement("name").text().toStdString();
                info.description  = root.firstChildElement("description").text().toStdString();
                info.state        = root.firstChildElement("state").text().toStdString();
                info.parent       = root.firstChildElement("parent")
                                        .firstChildElement("name").text().toStdString();
                info.creationTime = root.firstChildElement("creationTime").text().toLongLong();
            }
            free(xml);             // libvirt が確保した文字列を解放
        }

        // current（現在地）かどうか
        info.isCurrent = (virDomainSnapshotIsCurrent(snaps[i], 0) == 1);

        result.push_back(info);             // 配列へ追加
        virDomainSnapshotFree(snaps[i]);    // 要素ごとに解放
    }

    free(snaps);       // 入れる”本体”を解放
    virDomainFree(dom);
    return result;
}

// スナップショットを用いた復元関数
bool revertSnapshot(
    const std::string& vmName,
    const std::string& snapshotName,
    std::string* errorMessage
)
{
    LibvirtConnection conn;
    if (!conn.isValid()) {
        setError(errorMessage, "Failed to connect to hypervisor");
        return false;
    }

    virDomainPtr dom = virDomainLookupByName(conn.get(), vmName.c_str());
    if (!dom) {
        setError(errorMessage, "Domain not found");
        return false;
    }
    
    // 名前でスナップショットを検索
    virDomainSnapshotPtr snap = virDomainSnapshotLookupByName(dom, snapshotName.c_str(), 0);
    if (!snap) {
        setError(errorMessage, "Snapshot not found");
        virDomainFree(dom);
        return false;
    }

    int ret = virDomainRevertToSnapshot(snap, 0);   // 復元実行

    virDomainSnapshotFree(snap);
    virDomainFree(dom);

    if (ret != 0) {
        setError(errorMessage, "Failed to revert snapshot");
        return false;
    }
    return true;
}

// スナップショットの削除
bool deleteSnapshot(
    const std::string& vmName,
    const std::string& snapshotName,
    std::string* errorMessage
)
{
    LibvirtConnection conn;
    if (!conn.isValid()) {
        setError(errorMessage, "Failed to connect to hypervisor");
        return false;
    }

    virDomainPtr dom = virDomainLookupByName(conn.get(), vmName.c_str());
    if (!dom) {
        setError(errorMessage, "Domain not found");
        return false;
    }

    virDomainSnapshotPtr snap = virDomainSnapshotLookupByName(dom, snapshotName.c_str(), 0);
    if (!snap) {
        setError(errorMessage, "Snapshot not found");
        virDomainFree(dom);
        return false;
    }

    // flags=0：このスナップショット単体を削除（子があると失敗）
    int ret = virDomainSnapshotDelete(snap, 0);

    virDomainSnapshotFree(snap);
    virDomainFree(dom);

    if (ret != 0) {
        setError(errorMessage, "Failed to delete snapshot");
        return false;
    }

    return true;
}


