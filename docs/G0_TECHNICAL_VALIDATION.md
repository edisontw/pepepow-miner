# G0：原生 CPU 技術驗證

更新：2026-11-04（台灣）  
狀態：待執行。此文件是驗收計畫，沒有代表任何測試已通過。

## 目標與範圍

建立可重現的 Windows／Linux 原生 CPU hash harness，證明 HooHash V110 PEPEPOW variant 與現行 Core 完整一致。G0 先使用離線向量及有來源的鏈上 headers，不需要錢包私鑰或連入礦池。

第一版驗收目標為 Windows 11 x64、Ubuntu 22.04／24.04 x64。Windows 10、ARM64 及 GPU 待各自取得證據後列入支援。

## 下一個最小交付

1. 重新讀取 Core 與 pool 的最新分支 SHA。
2. 逐檔檢查 hash、BLAKE3、openlibm 與測試工具的來源、授權、依賴及編譯選項。
3. 固定採用 SHA，建立來源清單及第一組具有獨立 expected digest 的完整 32-byte KAT。
4. 建立可在 Windows／Linux 原生執行、任何 mismatch 回傳非零狀態的 CLI harness。
5. 再擴充測例與 CI，輸出 parity 報告。

先完成第一組完整 KAT 只代表初步驗證，不得將 G0 標為完成。

## 來源鎖定

2026-11-04 已重新讀取 Core master 與 pool main，與下列歷史 SHA 相同。它們是本次靜態查核快照，**不是本 repo 已採用或完成授權審查的依賴**。來源定位與差異見 [研究查核表](RESEARCH_LEDGER.md)。

| 專案 | 本次查核 SHA | 用途 |
| --- | --- | --- |
| MattF42/PePe-core | 38e801428688dfd0aee272b4a91819ac1a13ab4e | 共識參考及 HooHash 規格 |
| edisontw/pool-pepepow | c1298d7b40e5e9fd39f76af179c6e10a1c313147 | Stratum、序列化及向量參考 |

參考入口：

- https://github.com/MattF42/PePe-core
- https://github.com/edisontw/pool-pepepow

每個實際引入的檔案須記錄 repository、commit、path、檔案 checksum、license／notice、patch、建置相依；保留原始授權標示。未完成查核前不宣稱可直接重新散布。

## Harness 與測例

每個向量需記錄：

- 穩定 case ID、恰好 80 bytes 的 header hex。
- 參考來源、commit，以及 expected digest 的產生方式與完整 32 bytes。
- digest 的 raw-byte 順序，另行標明 RPC／display hash 的轉換方式。
- 真實鏈上案例的高度、block hash、header 來源與取得時間。
- 若為合成 header，標為合成資料，不冒充符合 PoW 的真實區塊。

驗收至少 10,000 組可供 hash 輸入的 header／nonce，包含真實鏈上 headers、nonce 邊界與欄位變動。hash 正確性不要求每個合成 header 都滿足 network target。

Expected digest 不可由待測實作在測試當下自行生成。固定已知答案與獨立建置的 Core reference，分別提供基礎 KAT 及差異比對。

| 檢查 | 必要證據 |
| --- | --- |
| 全長比對 | 每例比對全部 32 bytes；不只前綴 |
| 失敗行為 | 故意改壞末端 digest byte 後，程序與 CI 必須失敗 |
| 輸入格式 | 長度錯誤、非法 hex、截斷輸入均被拒絕 |
| nonce | offset 76–79 的 little-endian，涵蓋 0、1、最大值 |
| seed／cache | nonce 及其餘 header 欄位變動，對照 Core 的 seed 與失效規則 |
| 多執行緒 | 每個 worker 結果一致；獨立記錄可重現輸入 |
| 浮點 | 記錄 FP64、rounding、FMA／fast-math 與數學函式庫設定 |
| 可攜性 | 各 OS 原生執行，完整輸出工具鏈與平台識別 |

本次已確認固定 pool SHA 的 verify_known_vector.c 仍只比前 8 bytes、失敗仍 return 0（見查核表 R05）。不能直接把該工具的成功退出當成完整 parity 證據。

## 補充的來源與 coverage 驗收

- Core 的 dev_vectors.cpp 只生成 100 組合成輸入並呼叫 hash；可作 corpus 起點，不能代替獨立 expected digest、10,000 組或真實鏈上資料。
- 除 nonce 0、1、最大值，加入 255、256，並覆蓋 nonce 低 8 bits 全部 256 種值。每個 header 欄位獨立變動，驗證 seed/cache 失效；中間值至少覆蓋 firstPass、matrixSeed、選定 matrix cells 與 final output。
- Nonlinear branch／NaN/Inf 路徑以可重現向量或 instrumented reference 檢查；未能觸發的路徑明列 coverage 缺口，不能虛稱覆蓋。
- 明確分開 matrix multiplication output、hoohashv110 output 與 RPC/display encoding；後者函式已做 reverse，adapter 不可再次反轉。
- Windows Server CI 或交叉編譯不能冒充 Windows 11 原生驗收。三個目標 OS 分別提交 debug/release 結果與實際環境，缺平台保持 pending。

## 浮點與建置

以 Core 現行參考的數學函式及編譯語義為依據。openlibm 不得未經驗證即以 Windows 系統 libm 取代。最佳化、FMA 或 fast-math 的變更，每次都必須重新通過完整 parity。

記錄 OS、架構、CPU、compiler／版本、CMake、flags、相依版本及 build log。Windows 交叉編譯成功不等於 Windows 原生執行通過。CI 未執行或缺少設備時標記 pending。

## G0 完成定義

- [ ] 採用來源與依賴已固定 SHA，授權／notices 已整理。
- [ ] Harness 比對完整 32 bytes，錯誤與 mismatch 回傳非零。
- [ ] 故意破壞最後一個 digest byte 可使測試失敗。
- [ ] 至少 10,000 組測例全數對上獨立 reference，含鏈上案例。
- [ ] Windows 11 x64 原生 build／run 證據完整。
- [ ] Ubuntu 22.04 x64 原生 build／run 證據完整。
- [ ] Ubuntu 24.04 x64 原生 build／run 證據完整。
- [ ] 重現指令、測例來源、環境及結果報告已提交。
- [ ] 初步效能、相依問題、維護成本與 G1 backlog 已整理。

任何已知 mismatch 都阻止 G0 通過。未解決時記錄最小重現案例，沿序列化、BLAKE3、matrix／seed、浮點與編譯設定定位，不用顯示算力或成功連線替代證明。
