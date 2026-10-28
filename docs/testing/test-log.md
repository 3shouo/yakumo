# 手動テスト記録（yakumo-gui）

yakumo-gui には自動テストがなく、動作確認は実機（libvirt + KVM）への手動テストで行っている。
このファイルはその実施記録。新しいものを上に追記する。

## 書き方

1 回のテストセッションごとに 1 項目。テンプレート:

```
## YYYY-MM-DD タイトル
- ブランチ / コミット: feature/xxx（abc1234）
- 対象: 何を確認したか（checklist.md の ID があれば併記）
- 環境: 通常と違う点だけ書く（省略可）
- 手順と結果:
  1. 操作 → 実際に観察した結果
- 気づき: 想定外の挙動、見つかったバグ、作業候補
- 判定: 合格 / 不合格 / 一部保留
```

- 「結果」は期待ではなく実際に見えたことを書く（例: 「数十秒後に shutoff になった」）。
- バグを見つけたら、修正コミットが入った時点でその項目に追記する。
- 2026-10-28 より前の項目は作業メモと git 履歴から復元したもので、細部が抜けている可能性がある。

## 共通環境

- ホスト: Ubuntu Desktop、libvirt 10.0.0、接続先 `qemu:///system`
- テスト用 VM: OS 入りの myvm.qcow2 をコピーした専用ディスク（例: test-vm03.qcow2）を使う。1 つの qcow2 を複数 VM で共有しない。
- GUI は Qt Creator から起動。ログは qDebug（コンソール）と `journalctl -u libvirtd` で確認。

---

## 2026-10-28 createVM の domain XML に ACPI を追加
- ブランチ / コミット: feature/acpi（b36d960）
- 対象: CR-01, CR-02, LC-02
- 手順と結果:
  1. `<apci/>` のタイプミスがある状態で CreateVM（test-vm03 / 2048 MB / 2 vCPU / test.qcow2）→ 「Failed to define domain」。`journalctl -u libvirtd` に `unsupported configuration: unexpected feature 'apci'` が記録されていた。
  2. `<apic/>` に修正。myvm.qcow2 をコピーした test-vm03.qcow2 を指定して CreateVM → 作成成功。
  3. `virsh dumpxml test-vm03 | grep -A3 features` → `<acpi/>` と `<apic/>` を確認。
  4. Start でログイン画面まで起動 → Shutdown → 数十秒後に行をクリックするとボタン配置が shutoff 用に切り替わり、その後の定期更新（70〜90 秒）で一覧の状態列も shutoff になった。
- 気づき:
  - createVM 失敗時の文言に libvirt の理由が付かない（ライフサイクル 4 関数には付けてある）→ 作業候補。開発者向けには有用だが、一般ユーザーの需要は未定。
  - 行クリックは詳細パネルとボタンだけ更新し、一覧の状態列は定期更新待ちになる（`on_vmTable_cellClicked` が `updateDetail` だけを呼ぶため）。小さな不整合。
  - test.qcow2 は空ディスク（20 GiB / 実サイズ 6 MiB、OS 無し）なので起動・停止のテストには使えない。
- 判定: 合格

## 2026-10-27 UI 改善 ③④（復元）
- ブランチ / コミット: feature/ui-improvements → main に ff マージ（④ c35d9b6、③ efca672）
- 対象: SN-02, DEL-01, DEL-02, DEL-03
- 手順と結果:
  1. ④ スナップショット名を空（空白のみを含む）で OK → 警告ダイアログ。キャンセル → 無言で閉じる。
  2. ③ 稼働中の VM で Delete → 確認ダイアログを出さずに理由を表示。
  3. ③ スナップショットが残る VM で Delete → 同様に理由を表示。
  4. ③ 削除可能な VM で Delete → 確認ダイアログ → 削除成功（test-vm03 を実際に削除）。
- 判定: 合格

## 2026-10-26 UI 改善 ②（復元）
- ブランチ / コミット: feature/ui-improvements（3f06f0c）
- 対象: LC-05
- 手順と結果:
  1. VM の状態ごとにボタンの有効/無効が切り替わることを確認（shutoff 中は ForceStop が押せない等）。
- 気づき: Shutdown / Reboot を押してもゲストに何も起きない。原因は生成 XML に `<features><acpi/>` が無く、i440fx で ACPI が無効のまま QEMU が起動していたため → feature/acpi で対応。
- 判定: ボタン制御は合格。ACPI は別作業。

## 2026-10-19 libvirt エラー理由の取得順序（復元）
- 対象: LC-06 の準備（UI 改善 ①）
- 手順と結果:
  1. 失敗した API の後に `virDomainFree` を呼んでから `virGetLastErrorMessage()` → "no error" になる（libvirt 10.0.0）。
  2. Free の前に取得 → "Requested operation is not valid: domain is not running" のような理由文が取れる。
- 対応: 96023ac（2026-10-20）で 4 関数とも Free より前に理由を取る順序に統一。
- 判定: 検証完了

## 2026-10-16 ウォッチドッグ v1（復元）
- ブランチ / コミット: feature/watchdog（2fe9ddd、7e22831）
- 対象: WD-01, WD-02, WD-03
- 手順と結果:
  1. `pkill -9` で qemu プロセスを落とす → 自動再起動、ステータスバーに復旧通知、一覧が即時更新。
  2. 4 回連続で落とす → 3 回まで再起動し、4 回目で諦め通知が 1 回だけ出る。
  3. 諦めた後に手動 Start → 再アームされ、再び自動再起動する。
- 判定: 合格

## 2026-10-03 コア IF 化 第 2 段階後の通しテスト（復元）
- ブランチ / コミット: main（519e67b）
- 対象: VM 操作・スナップショット操作の一通り
- 手順と結果:
  1. 全操作を一通り実行 → 機能面は問題なし。
- 気づき: 稼働中 VM へのスナップショット作成（RAM 込み）で `virDomainSnapshotCreateXML` が長時間ブロックし、OS の「応答なし」ダイアログが出る。待てば完了する。IF 化以前からの既存問題 → 作業候補（ワーカースレッド化）。
- 判定: 合格（既知問題 1 件）

## 2026-09-25 メモリ表示の単位（復元）
- 対象: 一覧・詳細のメモリ表示
- 手順と結果:
  1. 一覧のメモリが実際の 1024 倍で表示される（`virDomainInfo.maxMem` は KiB 単位）。
- 対応: d7cfeed（2026-09-26）で `info.maxMem / 1024` に修正。
- 判定: 不合格 → 修正済み

## 2026-07-11 スナップショット機能（復元）
- ブランチ / コミット: 0c935e4
- 対象: SN-01, SN-03, SN-04, SN-05
- 手順と結果:
  1. 停止中 VM と稼働中 VM で Create / List / Revert / Delete を実行 → 動作。
  2. 子を持つスナップショットを Delete → 子が reparent され削除成功（内部スナップショットの仕様）。
- 対応: 「子があると失敗」と書いていたコメントを 4103e91 で訂正。
- 判定: 合格
