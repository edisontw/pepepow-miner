# G0-01：來源差異與 C harness 前端

日期：2026-11-04。接續 [來源盤點](G0_SOURCE_AUDIT.md)；G0 尚未通過。

## 更正前輪授權判斷

重新讀取 upstream LICENSE 的 commit history 後確認，Core 引用的
[9634f11410a2d71be21086e813263fa007fb6810](https://github.com/HoosatNetwork/hoohash/commit/9634f11410a2d71be21086e813263fa007fb6810)
本身就是 Toni Lukkaroinen 在 2025-10-14 的改授權提交：
「Change GPLv3 to MIT for more permissive algorithm usage.」
此姓名也出現在原始檔作者欄位，提交只將 LICENSE 從 GPLv3 改成 MIT。

因此不能繼續描述為「沒有可追溯的改授權依據」或要求所有離線 harness 工作停下。
已確認的是明確的 upstream 改授權意圖；舊檔頭確實未同步。
後續引入 Core reference 時須一併保留 Core/Hoosat notices、此 commit 依據與檔頭差異紀錄。
本次不替整個產品選擇 LICENSE，不把這項證據擴張為全部依賴或二進位發行審查已完成。

## BLAKE3 三個差異檔的判讀

比較基準：Core `38e801428688dfd0aee272b4a91819ac1a13ab4e` 的
`src/crypto/`，與 BLAKE3 `54930c95227daaac4dcf1eb3028e2f4e0768d139` 的 `c/`。
固定 blob ID 已在 [manifest](sources/g0-source-manifest.json)。

| 檔案 / symbol | 實際差異 | 對 G0 的意義 |
| --- | --- | --- |
| blake3.c / chunk_state_update | 最後一次 fill_buf 保留，但刪除其回傳值暫存與後續 input/input_len 更新；另修一處註解重字 | 更新值之後即離開函式，靜態判讀為移除無後續用途的更新；仍須 vectors 驗證 |
| blake3_impl.h | ARM64EC 從 IS_X86_64 排除，納入 IS_AARCH64 | 平台分類差異；不能當作演算法更動或 ARM64EC 已支援 |
| blake3_dispatch.c | 三個公開 dispatch 函式固定呼叫 portable；simd_degree 回傳 1；移除實際 CPU feature 選擇；調整 MSVC include guard | Core 是 scalar portable baseline，不可套用 upstream SIMD throughput 假設 |

這是完整檔案靜態比較的分類，不是 BLAKE3/KAT runtime parity。
不要為了「還原 upstream」先改掉 Core 參考；先以固定 portable baseline 建立獨立答案。

來源：[Core crypto](https://github.com/MattF42/PePe-core/tree/38e801428688dfd0aee272b4a91819ac1a13ab4e/src/crypto)、
[BLAKE3 C](https://github.com/BLAKE3-team/BLAKE3/tree/54930c95227daaac4dcf1eb3028e2f4e0768d139/c)。

## 本輪程式交付

- [tools/kat.c](../tools/kat.c)：嚴格解析 160/64 hex、全 32-byte 比對、mismatch exit 1、輸入錯誤 exit 2。
- [CMakeLists.txt](../CMakeLists.txt)：實際 reference 必須明確提供；synthetic test target 為獨立 opt-in。
- [tests/harness](../tests/harness/test_contract.py)：17 個 parser/comparator/exit-code 測例；測試替身明確標示不是 HooHash。
- [建置與執行方式](../tools/README.md)。

這只交付 harness 前端。未連結 Core、未生成 expected digest、未將候選向量升格正式 KAT。
前端錯誤退出測試可先驗收，不能拿它冒充 HooHash 負向測試已驗收。

## 下一步

1. 以固定 Core source + BLAKE3 portable + openlibm 0.8.7 建立獨立 reference，保留來源與 notices；不可 system-libm fallback。
2. 將候選 header 對照 reference，記錄原始 function bytes、RPC/canonical 映射、鏈上來源；固定 expected 後接入此 harness。
3. 在真實 reference 上重跑末 byte 負向測試，再擴充 corpus 及 Windows/Ubuntu 原生測試。

## 實際執行結果

本機 native Windows x64，以 Zig 0.15.2/clang 20.1.2 的 -O0 與 -O2 各編譯一次，兩組各 17 個前端測試全部通過。編譯與測試程序退出碼均為 0。末 byte mismatch 返回 1，非法輸入返回 2 且未呼叫測試替身。

[環境、命令與完整測試紀錄](VALIDATION_EVIDENCE/G0/frontend-contract-2026-11-04.txt)。CMake 配置本身未測、Core hash 未測、Linux 未測；G0 狀態保持未通過。
