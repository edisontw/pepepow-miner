# 架構決策：研究整合基準

日期：2026-11-04（台灣）。狀態：**已採納的開發規劃，尚未實作**。
基準 main：`780d49568989415818f984e1141c06d465dc0dcb`。實際檔案只有 8 份 Markdown；沒有可沿用的 miner source、build、CI、tests 或 LICENSE。01／02 無法公開存取 repo 的限制已由本次授權讀取解除，不能再推定存在舊 Shell miner 或 GPU 核心。

本文件是架構決策來源；[Roadmap](ROADMAP.md) 是唯一階段狀態來源；[研究查核表](RESEARCH_LEDGER.md) 管理事實與未決問題。三份 [研究原文](README.md) 保留，與本文件衝突時不作為實作指令。

## ADR-001：原生核心與交付順序（採納）

- G0 使用 C reference adapter + CMake 的離線 CPU harness；G1 host/controller 採 C++20，保留 C hash 邊界。不先引入 Rust manager 或 GPU toolchain。
- 自建 host、job builder、協定層與測試；不整體 fork 礦工。任何第三方 hash/函式庫重用都先完成來源與授權盤點；自行重寫也不自動等於授權已解決。
- 交付順序 G0 → G1 → G2 → G3；CUDA G4、AMD G5 各自驗收。G0/G1 通過後可作 GPU proof-of-concept，但不阻擋 CPU MVP，不以研究中的 CUDA-first 排程取代主線。
- CPU reference 為 correctness oracle；測試 expected digest 必須來自獨立固定的 Core reference，不能由待測核心自產自驗。

理由：本 repo 沒有既有實作須遷移；先減少語言與工具鏈邊界，隔離 FP64、header 與 Stratum 問題。這是工程選擇，不是已完成的效能比較。

## ADR-002：桌面框架（有條件首選）

G2 首選 **Qt 6 + C++**，與 CLI 共用 application controller，Qt 僅屬 GUI target；headless 不依賴 Qt。Rust + Tauri 2 保留替代方案，不同時維護兩套 GUI。

G1 結束後、G2 功能開發前，以 Windows 11／Ubuntu 22.04／24.04 的最小打包 spike 驗收：乾淨安裝與解除安裝、繁中／英文、tray、start/stop、sleep/resume、依賴體積、授權與散布方式。全部有證據才正式鎖定 Qt 版本與封裝；若失敗，記錄原因後以同一清單評估 Tauri，更新此決策。尚未執行 spike，不宣稱 Qt 已驗證或授權審查完成。

G1 先採版本化 JSON 設定及 JSONL 事件；SQLite、常駐 service、HTTP API、updater 延後，以實際歷史查詢或自動化需求另立 ADR。JSONL 是事件格式，不預先承諾跨程序 IPC。

## ADR-003：工作與 backend 邊界（採納）

```text
CLI / GUI
  → Application controller (start/stop/config/health)
  → Pool manager + Stratum codec
  → Immutable JobSnapshot + header/target builder
  → Scheduler → CPU / future CUDA / selected AMD backend
  → Candidate validator → Submit queue → Pool response events
```

- JobSnapshot 保存 session/connection epoch、job ID、generation、header template、extranonce、ntime、share target、network target、模式與收款身份；backend 不持有 socket 或可變全域 target。
- PEPEPOW header nonce 是 **32-bit**。02 的 64-bit allocator 僅作內部範圍計數，不能寫出 64-bit wire nonce。配置不重疊區間，耗盡 2^32 前取得合法新 extranonce2/template；不得 wrap 重做，ntime rolling 須先確認 pool 規則。
- 區分「停止派發舊工作」與「舊 candidate 可否提交」。新 notify 可優先排新工作，但 clean_jobs=false 不自動等於舊 job 失效；有效期與 previous-job 規則由固定 pool fixtures 定義。clean_jobs=true、斷線／failover 或 extranonce 身份變更使相關工作失效；submit 前再次檢查 epoch 與有效 job。
- Difficulty 更新如何綁定 job 必須由 pool 原始碼與 race fixtures 確認；已建立的 snapshot 不得被新的全域 difficulty 覆寫。
- Matrix cache 以 nonce 歸零後的完整 header template + algorithm profile 識別。version、prevhash、merkle root、ntime、nbits 變動均不得沿用舊 matrix。
- GPU **每個待提交 candidate** 都經 canonical CPU 全 32-byte hash 與 target 複核；抽樣僅用於額外的 search correctness 診斷，不能代替 submit 前驗證。mismatch 隔離 worker 並保存重現輸入，不能只把 reject 隱藏。
- hash、RPC/display 與 target integer 的 byte order 分別命名；Core hoohashv110 已反轉輸出，adapter 不再盲目 reverse。詳見查核表。

## ADR-004：GPU 與產品範圍（採納／延後）

CUDA 是首個 GPU 實驗方向。AMD **尚不指定 HIP primary 或 OpenCL fallback**：G5 前以實際可取得的 Windows/Linux 卡型、FP64 parity、驅動、工具鏈、授權與維護成本選一條初始路線。不得從 NVIDIA telemetry 推定 AMD 能力，不因 API 名稱推定 HooHash bit-exact。

MVP：CPU CLI/GUI、明確 start/stop、手動資源 profile、readiness、事件式 Share Doctor、診斷匯出、基本同模式 reconnect/failover。POOL 與 pool-based SOLO 逐 endpoint 驗收，SOLO 不等於直接 wallet RPC。
自動活動偵測、效率自調、數值健康評分、收益追蹤、remote API、service/autostart 不列為 G2 阻擋項；缺資料顯示 unknown/N/A。

02 的 72h final soak 保留為正式穩定版門檻；G2/GPU 的 24h 驗收與 G3 Beta 不因此宣稱已完成 72h。任何 hash mismatch 都阻擋相關 backend 發行。

## 重開決策的條件

Core/pool SHA 或協定變更、授權無法釐清、三個目標 OS parity 失敗、GUI 打包 spike 失敗、實卡 backend 不可用時，提交具體證據並更新對應 ADR。沒有上述新證據不反覆更換技術棧。
