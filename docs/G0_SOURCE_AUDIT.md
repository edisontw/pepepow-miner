# G0-01：來源與候選向量查核

日期：2026-11-04。基準 main：`9e2c3c671f300ba8efaa72969b5172ad855328e8`。
狀態：**來源盤點已交付；授權確認、獨立 Core KAT、C harness 與原生 parity 仍 pending。Issue #1 不關閉，G0 不通過。**

## 交付

- [來源 manifest](sources/g0-source-manifest.json)：20 個固定 commit/path/Git blob ID，未複製第三方程式。
- [候選向量](../tests/vectors/candidates/pool-known-chain-001.json)：80-byte header、32-byte pool canonical digest 與來源 symbol。候選獨立於未來 miner，但尚未由獨立 Core 執行確認；刻意不填 Core output 或鏈上高度。
- 本文件記錄可重現的靜態結果及下一步，不代表執行過 hash。

## 發現與處置

### 1. HooHash 授權訊號衝突已定位到歷史 commit

Core hoohash.c 指向 Hoosat `9634f11410a2d71be21086e813263fa007fb6810`。
該 commit 的 [LICENSE](https://github.com/HoosatNetwork/hoohash/blob/9634f11410a2d71be21086e813263fa007fb6810/LICENSE) 是 MIT，
但 [hoohash.c](https://github.com/HoosatNetwork/hoohash/blob/9634f11410a2d71be21086e813263fa007fb6810/hoohash.c) 與
[hoohash.h](https://github.com/HoosatNetwork/hoohash/blob/9634f11410a2d71be21086e813263fa007fb6810/hoohash.h) 明列 GPL-3.0-or-later。
Core 自己的 COPYING／hoohash.c 則標 MIT。這是原始檔觀察，並非斷言哪一授權依法優先，也不是已完成相容性審查。

[AGENTS.md](../AGENTS.md) 規定「未驗證來源授權前不複製或發行第三方程式碼」；因此本交付不 vendor、不發布 binary、不擅自替專案選 LICENSE。
解除條件：取得可追溯的授權澄清或完成逐檔 lineage／相容性審查與採用決策。單純願意開源、改檔頭或重寫，不代替來源審查。

### 2. BLAKE3 不能僅依 version string 視為原版

Core blake3.h 為 1.5.1；以 upstream tag 的 commit
[54930c95227daaac4dcf1eb3028e2f4e0768d139](https://github.com/BLAKE3-team/BLAKE3/tree/54930c95227daaac4dcf1eb3028e2f4e0768d139) 比對 Git blob：

| 檔案 | 是否 byte-identical |
| --- | --- |
| blake3.h | 是 |
| blake3_portable.c | 是 |
| blake3.c | 否 |
| blake3_impl.h | 否 |
| blake3_dispatch.c | 否 |

上游 LICENSE 提供 CC0-1.0 或 Apache-2.0。尚須審查三個不同檔案的 diff／patch 來源與 notices；本次只確認 bytes 不同，未把差異推定為語義變更。

### 3. openlibm 版本已定位，但不能默默 fallback

Core 的 [depends recipe](https://github.com/MattF42/PePe-core/blob/38e801428688dfd0aee272b4a91819ac1a13ab4e/depends/packages/openlibm.mk)
指定 0.8.7，archive SHA-256 已抄入 manifest；tag 對應 `9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`。
未下載 archive，未宣稱 checksum 實測通過。

[openlibm LICENSE.md](https://github.com/JuliaMath/openlibm/blob/9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d/LICENSE.md)
列多種來源授權，包含 MIT/BSD/ISC、FDLIBM notices，部分測試為 LGPL；需要按實際建置／散布檔案分類。

Core configure.ac 找不到 openlibm 時會警告並 fallback 到 system libm。
未來 G0 build 必須驗證 HAVE_OPENLIBM、實際 linked library 與 flags，缺依賴時明確失敗，不以 configure exit 0 視為符合 deterministic profile。
src/Makefile.am 設定 no-fast-math、fp-contract=off、excess-precision=standard；x86 flags 由 configure 探測，須記錄最終命令，不能只記計畫 flags。

### 4. Pool 的兩個向量測試必須區分

- verify_known_vector.c：只 memcmp 前 8 bytes，FAIL 也 return 0。
- tests/test_stratum_ingress.py 的 test_pepepow_header_hash_matches_known_chain_vector：比較完整 64 hex chars。不能聲稱 pool 完全沒有全長比較。
- _calculate_pepepow_share_hash 將 native output reverse 一次再返回 canonical bytes；C 工具直接呼叫 matrix multiplication。兩者 API 層級不同，不能盲目共用 digest encoding。
- 測試名稱稱 chain vector，但目前未取得高度、RPC 原始 header 或獨立 Core 執行輸出，因此候選仍非正式鏈上 KAT。所有來源 permalink 與 blob ID 見 manifest。

## 本輪驗證結果

本機 PowerShell 執行 JSON 與長度檢查，exit 0：
`PASS: JSON parses; header=80 bytes; candidate digest=32 bytes; Core digest remains pending; 20 pinned source entries; no vendored code.`

重現（repo root，PowerShell）：

```powershell
$ErrorActionPreference = 'Stop'
$v = Get-Content -Raw 'tests/vectors/candidates/pool-known-chain-001.json' | ConvertFrom-Json
$m = Get-Content -Raw 'docs/sources/g0-source-manifest.json' | ConvertFrom-Json
if ($v.header_hex -cnotmatch '^[0-9a-f]{160}$') { throw 'Invalid header' }
if ($v.expected_pool_canonical_hex -cnotmatch '^[0-9a-f]{64}$') { throw 'Invalid digest' }
if ($null -ne $v.expected_core_function_output_hex) { throw 'Unverified Core digest' }
foreach ($entry in $m.entries) {
    if ($entry.commit -notmatch '^[0-9a-f]{40}$' -or
        $entry.git_blob_sha1 -notmatch '^[0-9a-f]{40}$' -or $entry.vendored) {
        throw 'Invalid source entry'
    }
}
```

| 檢查 | 結果 |
| --- | --- |
| JSON/長度/來源 ID 格式 | passed（只驗資料格式） |
| Core/pool 分支重新查核 | SHA 與 manifest 一致 |
| HooHash/BLAKE3/openlibm 授權整合 | pending |
| C/CMake harness build、32-byte KAT、末 byte 負向測試 | pending，未執行 |
| Windows 11／Ubuntu 22.04／24.04 native parity | pending |
| pool／GPU 測試 | 未執行，不在本工作包 |

環境探測：PATH 未找到 cmake、clang、gcc、cl；常見 Visual Studio／LLVM／MSYS2／CMake 位置未找到安裝。這只反映本次可見環境，未完成全磁碟工具鏈盤點。未安裝系統工具或以其他平台冒充原生證據。

## 下一個可執行交付

1. 審查三個 BLAKE3 差異檔與 HooHash lineage；整理可採用的逐檔 notices／授權決策，必要時取得權利人澄清（本輪未對外聯絡）。
2. 建立可重現的 native C/CMake 工具鏈與獨立 Core reference；記錄 openlibm、compiler/flags。
3. 以候選 header 取得獨立 Core output，確認 canonical/raw 映射與鏈上來源，再升格正式 KAT。
4. 建立全 32-byte harness，最後一個 byte 故意修改必須非零退出；非法 hex、錯長、缺資料皆拒絕。之後才擴至 10,000 組。

來源查核可以繼續；第三方核心整合須先解決上列授權問題。不可把本交付視為 G0-01 完成。
