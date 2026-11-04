# PEPEPOW Miner — 產品開發計畫（03_PRODUCT_DEVELOPMENT_PLAN）

> 版本：2026-11-04 · 狀態：**規劃文件／未實作** · 適用：Windows 11 x64、Ubuntu 22.04/24.04 x64（第一輪目標，非已驗證支援）  
> 專案：`edisontw/pepepow-miner` · 相關文件：`docs/G0_TECHNICAL_VALIDATION.md`、`docs/ROADMAP.md`  
> 本文件整合 PEPEPOW Miner 深度研究與目前 repository 的實際內容，供產品、工程與 AI Coding Agent 逐項執行；**規劃不等於完成、推測不等於協定事實。**

## 0. 現況、範圍與決策紀律

截至 2026-11-04，本 repository 只有 README 與 G0／Roadmap 規劃文件，**沒有可用礦工程式、已完成 hash parity、實際 accepted-share 證據或正式發行檔**。任何 roadmap、競品比較與設計均不得被解讀為已經實作。後續每個階段必須提交可重現的測試與原始證據。

**已由 repository 確認：**
- 既定開發關卡為 G0 原生 CPU HooHash V110 正確性 → G1 CPU Stratum CLI → G2 桌面 MVP → G3 Beta 發行；NVIDIA G4、AMD G5 分別設 gate。
- G0 必須以獨立 reference 驗證至少 **10,000 組完整 32-byte hash**，涵蓋 Windows 11 x64、Ubuntu 22.04/24.04 x64 的**原生**執行，且故意改壞 digest 末位必須造成非零退出。
- G1 必須有真實 pool accepted share、可追溯 Stratum 事件、受控錯誤測例；accepted share 不等於找到區塊或收到付款。
- 只處理公開收款地址，不索取 seed／private key；必須由使用者明確啟動挖礦。POOL 與 pool-based SOLO 不得自動互換。
- 歷史參考來源 SHA：`MattF42/PePe-core@38e801428688dfd0aee272b4a91819ac1a13ab4e`、`edisontw/pool-pepepow@c1298d7b40e5e9fd39f76af179c6e10a1c313147`；這些**不是**本 repo 已鎖定的相依版本。

**待驗證／不可預設：**演算法精確序列化與 difficulty 基準、Stratum dialect、Core 與 pool 的最新 commit、參考實作授權與再散布權、GPU backend、支援卡型、GUI framework、native Windows AMD 監測能力、功耗控制權限、實際 pool 收益 API。所有決策先寫研究紀錄與 ADR，通過對應 gate 才能進 production code。

**產品願景：**PEPEPOW 專用的「Mining Assistant」，而非另一套只有 hashrate 圖表的礦工。核心承諾是：**一鍵開始、開始前預檢、share 品質可解釋、故障可安全復原、效率可量測、CLI/GUI 一致。**

## A. 競品研究、UX 缺口與差異化

### A1. 競品能力矩陣（2026-11-04 公開文件快照，非實機全面測試）

| 競品 | 強項／已有能力 | 使用者體驗／可借鏡之處 | PEPEPOW Miner 應避免的錯誤 |
| --- | --- | --- | --- |
| **HTN Miner** | HooHash V110、CPU/GPU 路徑、GPU 選擇、JSON 設定、backup Stratum、autotuner、pool-side hashrate、stale-work／clock-skew 處理 | CLI/config 為主；公開 FAQ 提及 Windows 主要使用 WSL、未規劃 native Windows build（後續須複核） | 不把 backup pool、autotune、顯示算力當作獨家創新；應以 native Windows／Linux 安裝與故障解釋取勝 |
| **Hoominer** | CLI 導向；公開 README 列 CPU、OpenCL、CUDA execution paths | 需依**特定幣別與版本**核對 backend：公開 v0.4.1 PEPEPOW 欄位列 CPU/OpenCL，不能推定其 PEPEPOW CUDA 已完整支援 | 不把「有 CUDA 模組」誤寫成「PEPEPOW CUDA 已驗證」 |
| **XMRig** | JSON config、offline benchmark、HTTP API、可限制 API 權限 | headless 與可程式化介面成熟；API 預設 localhost、可用 token | 不只複製 HTTP API；須將 API 安全預設、CLI parity 與可觀測性做到一致 |
| **GMiner** | watchdog、failover、每 GPU 溫度／功耗／效率、browser statistics | 豐富的 GPU telemetry 與維運選項 | 溫度／功耗面板不是差異化；需有根因診斷與安全復原 |
| **lolMiner** | benchmark、share latency 觸發 failover、watchdog、browser GUI | 進階網路容錯與操作可見性 | 不以單純 GUI、benchmark、failover 宣稱領先；應以 PEPEPOW 專用健康檢查與證據鏈為核心 |

以上是功能與 UX 比較，不代表競品一定支援 PEPEPOW 或特定 GPU／OS；每項支援須以官方版本矩陣和實測確認。研究入口見本文末參考資料。

### A2. 市場缺口與優先序

排序準則：**解決實際挖礦失敗的程度 > 對 PEPEPOW 的專屬性 > 可在 G0/G1 後驗收 > 維護成本 > 視覺吸引力**。P0＝MVP 必須；P1＝可靠性／效率版本；P2＝進階版本。

| 排名 | 功能 | 優先 | 核心差異／可驗收結果 |
| ---: | --- | :---: | --- |
| 1 | **Share Doctor** | P0 | 將 reject/stale/duplicate/local verify fail/network timeout/driver reset 分類，附來源事件、推論信心與建議 |
| 2 | **PEPEPOW Readiness Check** | P0 | wallet、pool、協定、時間偏差、設備能力、權限與網路逐項 preflight；不通過不得宣稱可挖 |
| 3 | **Safe Adaptive Efficiency** | P1 | 以 Hash/Watt、有效 accepted work、溫度與穩定性共同最佳化，支援回退；無量測不調參 |
| 4 | **Recovery Ladder** | P1 | reconnect → 同模式備援 pool → 隔離故障 worker → 受限重啟；保留原因與次數 |
| 5 | **Activity-Aware Mining** | P1 | Performance/Balanced/Interactive/Pause；自動偵測遊戲或影片延後至有平台證據 |
| 6 | **Pool Health Intelligence** | P1 | latency、stale/reject、斷線、恢復歷史、樣本數與遲滯門檻；不跨 POOL/SOLO |
| 7 | **Baseline/Regression Benchmark** | P1 | driver、版本、設定與硬體固定條件比較，避免更新後效能回退 |
| 8 | **Per-GPU Health Fingerprint** | P1 | 每張卡的 hashrate、功耗、溫度、share 品質、錯誤與能力缺口 |
| 9 | **Diagnostic Evidence Pack** | P0 | 一鍵輸出去識別化事件、環境、設定摘要、測試結果；先預覽再分享 |
| 10 | **Mining Health Score** | P0 | Healthy/Degraded/Action Required；公開權重、來源、樣本與缺失值 |
| 11 | **Electricity/Yield Analytics** | P2 | 明確區分實測耗電、估計耗電、pool 報酬、幣價與費率來源 |
| 12 | **Multi-rig Fleet** | P2 | 安全裝置註冊、只讀聚合、最小權限遠端操作、可撤銷金鑰 |
| 13 | **Native Windows/Linux Packaging** | P0 | 免 WSL 的原生安裝、清楚權限／解除安裝與自動啟動選擇 |
| 14 | **GUI/CLI 行為一致** | P0 | 同一 command/service 層與狀態機；可重現的 headless 使用方式 |

### A3. 優先鎖定的五個 PEPEPOW 專屬特色

**F1 Readiness Check（P0）**  
把「連得上」拆為 wallet format → DNS → TCP → TLS（適用時）→ Stratum handshake/authorize → algorithm/profile/target compatibility → GPU/CPU capability → 時間偏差。顯示 pass/warn/fail/unknown 與檢查依據。不得用沒有真正送出的 share 推論 pool 已接受。

**F2 Share Doctor（P0）**  
對每次 submit 建立 correlation ID，關聯 job ID、target、local verify、送出時間、pool response、retry/failover。診斷規則分為「可證實」與「可能原因」，不將 stale 一律歸因於網路，也不將 pool reject 視為本機 hash 一定錯誤。

**F3 Safe Adaptive Efficiency（P1）**  
只有可讀取可信功耗／溫度且完成 GPU correctness、穩定性基準後才開啟。先做只讀建議與保守的應用內負載／執行緒調整；涉及 power limit、clock、voltage 的控制須獨立安全審核、明確同意、硬體白名單與 rollback。

**F4 PEPEPOW Pool Health Intelligence（P1）**  
在同一挖礦模式、同一收款地址下觀察各 endpoint 的延遲、有效 share 比率與失聯頻率；需最小樣本、cooldown 與 hysteresis，防止 ping-pong 切池。區分 pool-side hashrate 與本機 hashrate。

**F5 Evidence Pack + Health Score（P0）**  
將每次故障的設定摘要、平台資訊、事件 timeline、檢查結果與排除敏感資料的 log 匯出。健康分數必須可追溯到具體規則，不可把缺少 telemetry 當作健康。

## B. 功能可行性、價值、風險與平台策略

### B1. 功能評估矩陣

可行性欄位表示**工程預估**，不是已完成的技術證明。平台差異均須依實際硬體驗證。

| 功能 | 使用者價值 | 技術可行性／依賴 | 主要風險與安全控制 | 時程 |
| --- | --- | --- | --- | --- |
| First-run wizard | 高：降低設定失誤 | 高；需 wallet/pool schema、設備探測、Stratum preflight | 不索取私鑰；預檢與開始挖礦分開授權 | MVP |
| One-click start/stop | 高：最短操作路徑 | 高；需可靠的 mining state machine | 明確開始、停止可預期、資源上限；不得隱藏背景挖礦 | MVP |
| Benchmark | 高：可比較設備 | 中；先 CPU 正確性，再 GPU；需固定輸入與環境 | 不以不正確 hash 換取高 H/s；避免長時間滿載無警示 | MVP 基本、P1 回歸 |
| Dashboard | 中高：可觀測 | 高；events + snapshots | 分開 local/pool hashrate、accepted/rejected/stale；顯示取樣時間 | MVP |
| 效率模式 | 高：降低電費／熱負荷 | 中低；取決於 telemetry、GPU correctness | 功耗估計與實測分開；保守預設、回退、白名單 | P1 |
| Gaming/interactive mode | 中高：兼顧日常使用 | 中；先手動 profile，後平台活動探測 | 誤判造成卡頓、頻繁切換；設冷卻期、使用者覆寫 | P1 |
| Failover | 高：維持可用 | 高；G1 協定狀態機後 | 不跨模式／不改錢包；延遲與 cooldown | MVP 基本、P1 智慧 |
| Error diagnosis | 高：降低支援成本 | 中；需結構化事件與 fixtures | 避免誤診；每個推論附來源、信心與下一步 | MVP |
| Multi-GPU | 高：礦機實用性 | 中；GPU backend 完成後 | 單 GPU 故障隔離、device ID 穩定性、驅動相容 | GPU gate 後 |
| Multi-rig | 中：進階使用者 | 中低；需要安全註冊與網路設計 | 遠端攻擊面、憑證洩漏、命令授權 | P2 |
| Profitability | 中：決策資訊 | 中低；需可信 pool/network/price/費率來源 | 幣價波動、預估誤差、不能保證獲利 | P2 |
| Local API | 高：可自動化 | 高；application services 完成後 | 預設 localhost、read-only、token、速率限制 | P1 |
| Remote API/alerts | 中高：無人值守 | 中；需 TLS、認證、審計 | 預設關閉；不暴露到 0.0.0.0；不得傳送敏感設定 | P1/P2 |
| Tray/background | 中高：桌面操作 | 高；Windows/Linux GUI framework 待 ADR | 清楚提示正在挖礦、退出與停止行為分開 | MVP/P1 |
| Autostart | 中：方便重啟 | 高；平台原生機制 | 預設關閉；登入啟動不代表自動挖礦，需獨立同意 | P1 |
| CLI/headless | 高：Linux／遠端主機 | 高；核心與 UI 解耦 | 可安全 stop、設定驗證、非零退出、機密遮罩 | G1 起 |

### B2. 平台與硬體支援矩陣

| 類別 | Windows 11 x64 | Ubuntu 22.04/24.04 x64 | 開放條件 |
| --- | --- | --- | --- |
| Native CPU hash/CLI | G0/G1 目標，未驗證 | G0/G1 目標，未驗證 | 完整 KAT、真實 accepted shares |
| GUI、tray、安裝包 | G2/G3 目標 | G2/G3 目標；桌面環境差異須測 | clean install、sleep/restart、uninstall |
| NVIDIA GPU mining | G4 待驗證 | G4 待驗證 | CUDA ADR、per-device parity、實際 share、24h |
| NVIDIA telemetry | 可評估 NVML | 可評估 NVML | driver/API 實測；不支援欄位顯示 N/A |
| AMD GPU mining | G5 [TBD] | G5 [TBD] | OpenCL/HIP 等方案 ADR + 實卡測試 |
| AMD telemetry/control | **[TBD]**，不能預設 AMD SMI 原生 Windows 可用 | 依 AMD SMI／driver 支援矩陣評估 | 逐型號驗證，未支援時降級只讀或 N/A |
| ARM64、Windows 10、其他 Linux | 不列入首輪承諾 | 不列入首輪承諾 | 獨立 build、parity、runtime 證據 |

NVIDIA NVML 提供跨 Windows/Linux 的 GPU state、utilization、temperature、power 等能力，但每個欄位仍依 GPU/driver 支援。AMD SMI 官方文件主要以 Linux bare metal/VM 為目標；WSL 路徑另有實驗性限制。不得把 NVIDIA 的可行性直接套用 AMD。

### B3. 建議架構（**分層原則已建議，實作技術未決定**）

`Mining Core → Platform Services → Application Services → CLI / Desktop GUI / Local API`

- **Mining Core**：hash reference adapter、nonce/search、share verifier、Stratum state machine、worker scheduling；任何 GUI 不得直接修改 nonce 或 target。
- **Platform Services**：GPU/CPU 探測、telemetry、OS lifecycle、notifications、secure config、檔案路徑、日志輪替。
- **Application Services**：profile、start/stop、readiness、share doctor、recovery、benchmark、health score；使用共同狀態機與 typed events。
- **Surfaces**：CLI、GUI、localhost API 使用同一 command layer；Linux headless 不依賴 GUI。
- **資料模型**：versioned config、structured JSONL events、session ID、job/share correlation ID、metrics snapshot；SQLite 等持久化方案須 ADR。

**候選但未鎖定**：C/C++ + CMake native core、Rust manager、Tauri 2 + TypeScript GUI、SQLite、CUDA/OpenCL/HIP。須以授權、維護成本、可攜性、打包體積、崩潰隔離與效能實測決策，不能因為列在文件就當作已選定。

**狀態機**：`Idle → Preflight → Ready → Starting → Mining → Degraded / Recovering → Stopping → Idle`，另有 `Error`。每次 transition 需可記錄、可測試；未經使用者同意不得從 Idle 自動進 Mining。

### B4. 安全、隱私與穩定性不可退讓規則

1. 只收收款地址，不要求私鑰、seed phrase 或 wallet RPC 憑證；地址檢查以現行 Core 規則為準。
2. 只有明確操作才開始挖礦；預設不在開機／登入時暗中挖礦；UI/tray/CLI 均可立即停止。
3. Pool failover 僅在相同 mining mode、相同 payout identity 下進行；不得自動 POOL ↔ pool-based SOLO。
4. API 預設關閉或僅綁定 `127.0.0.1`；寫入命令須認證與授權；遠端功能獨立 opt-in、審計、可撤銷。
5. Telemetry、錯誤回報、benchmark submission、雲端同步預設關閉；先告知資料種類與用途。
6. Log/診斷包遮罩 wallet 識別資訊（可保留部分尾碼供核對）、tokens、主機名稱、IP 與個資；分享前預覽。
7. 功耗、溫度與時脈控制以硬體可用性和使用者同意為 gate；預設只讀，不做自動超頻／電壓操作。
8. 自動恢復要有限次數、退避、冷卻期與人工停止；不允許無限制重啟、未授權系統 reboot。
9. 任何未知演算法、difficulty、endianness、Stratum 欄位須 `blocked-research`，不得憑猜測實作。
10. 簽章、checksum、SBOM、授權 notice、release reproducibility 與供應鏈掃描列入正式發行 gate。

## C. PEPEPOW UX：主要流程與 wireframes

### C1. 首次使用流程

`歡迎／風險說明 → 裝置與 OS 能力探測 → 填入收款地址 → 選 POOL 或 pool-based SOLO（明確解釋）→ 設定 endpoint → Readiness Check → 保守 benchmark（可跳過）→ 顯示資源模式與預估影響 → 使用者按「開始挖礦」`

Preflight fail 時不顯示「一鍵開始成功」，而是顯示具體阻礙及可執行修正。連線成功但尚無 accepted share，狀態應是「連線正常，挖礦結果待確認」。

### C2. 首次啟動／Readiness Check

```text
┌───────────────────────────────────────────────────────────────────┐
│ PEPEPOW Miner                              Step 4/5 · Readiness  │
├───────────────────────────────────────────────────────────────────┤
│ 收款地址   ✓ 格式符合目前已驗證規則       [檢視遮罩地址]          │
│ 模式       ✓ POOL（不會自動改成 SOLO）                            │
│ DNS/TCP    ✓ 已連通                     延遲  [實測值]          │
│ Stratum    ? 尚未完成握手／授權            [重試] [查看證據]      │
│ 演算法     ? profile 尚未由 G0/G1 驗證                            │
│ GPU        — 未啟用／尚無支援證據                                │
│                                                                   │
│ 狀態：尚未準備就緒；請先完成協定驗證                              │
│ [返回設定] [匯出診斷]                      [開始挖礦（停用）]       │
└───────────────────────────────────────────────────────────────────┘
```

### C3. 日常 Dashboard：優先健康、其次數值

```text
┌───────────────────────────────────────────────────────────────────┐
│ PEPEPOW Miner        ● Mining / Degraded          [暫停] [停止]  │
├───────────────────────────────────────────────────────────────────┤
│ Mining Health：DEGRADED   原因：最近 stale share 增加             │
│ Hashrate  [本機 H/s]   Accepted [n]  Stale [n]  Rejected [n]    │
│ Power     [W / N/A]     Efficiency [H/W / N/A]   取樣 [時間]     │
├───────────────────────────────────────────────────────────────────┤
│ 裝置        H/s       溫度     功耗      Shares       狀態       │
│ CPU #0      ...       N/A      N/A       ...         Mining     │
│ GPU #0      ...       ...      ...       ...         Healthy    │
├───────────────────────────────────────────────────────────────────┤
│ Share Doctor：2 個可追蹤事件                                      │
│ 14:02  Stale   job 已過期／pool response  [證據] [處理建議]       │
│ 14:07  Timeout TCP 斷線 1 次              [證據] [重試歷史]       │
│ [完整事件時間線] [匯出 Evidence Pack] [設定]                      │
└───────────────────────────────────────────────────────────────────┘
```

所有 `[n]`、`[時間]` 均為 UI 佔位符，不是測試結果。當 telemetry 缺失，顯示 `N/A`，不得補假數字。Hashrate、pool-side estimate、有效難度加權 work 必須各自標示。

### C4. Share Doctor／Recovery Ladder

```text
┌───────────────────────────────────────────────────────────────────┐
│ Share Doctor / Session 2026-11-04-...                             │
├───────────────────────────────────────────────────────────────────┤
│ 問題：Stale share 增加           信心：中（資料不足）              │
│ 證據：job=...；提交時間=...；pool response=...；RTT=...           │
│ 可能原因：工作更新延遲、網路波動、worker 未及時丟棄舊 job          │
│ 已採取：重新連線 1/3；下次嘗試 30 秒後；未切換挖礦模式             │
│ 建議：檢查系統時間、pool latency、clean_jobs handling            │
│ [查看原始事件] [停用自動恢復] [匯出證據]                            │
└───────────────────────────────────────────────────────────────────┘
```

### C5. CLI/headless 行為契約（命令名稱暫定）

```text
pepepow-miner doctor --config ./miner.json
pepepow-miner benchmark --device cpu --duration 60s
pepepow-miner start --config ./miner.json --profile balanced
pepepow-miner status --json
pepepow-miner stop
pepepow-miner export-diagnostics --redact
```

CLI flags、JSON schema 與 exit codes 在 G1/G2 實作時以 ADR 鎖定。建議退出碼：0 成功、2 設定錯誤、3 網路／協定錯誤、4 裝置不可用、5 correctness failure；不得把範例視為已存在的指令。

## D. 可執行 Roadmap、依賴與驗收

### D1. 與既有 G0–G5 的關係

**現有 `docs/ROADMAP.md` 是交付關卡的優先來源**；以下 M0–M9 是更細的產品工作分解，不能繞過 G0/G1 或提前宣稱 GPU 支援。主依賴：`M0 → M1(G0) → M2(G1 mock) → M3(G1 live) → M5(platform) → M6(G2 MVP) → M7(G3 reliability/beta) → M8(adaptive) → M9(fleet)`；`M4(G4/G5 GPU)` 在 M1/M3 正確性 gate 之後獨立推進，完成前 GUI 只顯示已驗證的 CPU 能力。

| Milestone | 可交付成果 | 明確驗收／不得宣稱 |
| --- | --- | --- |
| **M0 Research Lock** | `RESEARCH_LEDGER.md`、`ASSUMPTIONS.md`、來源/授權清單、協定 fixtures、ADR backlog | Core/pool 最新 SHA 與授權逐項查證；未知標 TBD；不得憑推測寫演算法 |
| **M1 Correctness Harness = G0** | Native CPU harness、KAT、差異比對、build manifests | ≥10,000 全 32-byte parity；digest 末 byte 故意錯誤使測試 fail；Windows 11 + Ubuntu 22.04/24.04 原生跑過 |
| **M2 Stratum State Machine** | subscribe/authorize/job/target/submit/reconnect、mock pool | endianness、difficulty boundary、clean_jobs、old job、duplicate、timeout、TLS（適用時）fixtures 全通過 |
| **M3 Live Interoperability = G1** | CPU CLI 對 PEPEPOW pool 的實際互通 | pool 回覆 accepted shares；回覆/拒收來源可追；長時 soak；每主要配置至少 1,000 筆已回覆 share 才評估拒收率（可用受控低難度環境） |
| **M4 GPU Backend = G4/G5** | GPU ADR、選卡、local verify、隔離與 telemetry | 每 OS/卡型/driver 逐一 parity、實際 accepted shares、24h 觀察；沒有證據的卡型不列支援 |
| **M5 Cross-platform** | Windows/Linux native CI、設定 schema、平台抽象 | clean build/run；配置遷移、錯誤退出、可重現環境資訊；AMD telemetry 未驗證保持 N/A |
| **M6 Desktop MVP = G2** | wizard、one-click、health dashboard、Share Doctor、基本 benchmark、CLI parity、診斷包 | 全新安裝；可停止、sleep/resume、斷線恢復；各 OS 24h 觀察；繁中/英文；不洩漏敏感資料 |
| **M7 Reliability/Beta = G3** | recovery ladder、同模式 failover、tray、local API、Linux service、封裝 | 可重現 build、checksum/SBOM/授權 notice、安裝/解除安裝、API 安全測試、release notes、支援矩陣 |
| **M8 Adaptive** | baseline/regression、safe efficiency、interactive profile | 同硬體可重現比較、溫度/功耗安全邊界、rollback、no-telemetry 降級 |
| **M9 Advanced** | fleet、遠端警報、收益分析、signed update（如採用） | opt-in、認證、撤銷、審計、威脅模型、可回退更新；不得宣稱保證獲利 |

**MVP 定義**：「能正確挖、能看懂、能清楚失敗」。MVP **不包含**自動電壓操作、cloud account、多礦機集中控制、自動遊戲偵測、直接 wallet/RPC SOLO、多幣交易或未驗證 GPU 家族。

### D2. 可立即開立的 GitHub Issues（建議順序）

每個 Issue 都必須標明 Goal、Context、In scope、Out of scope、Acceptance criteria、Test commands、Human validation、Evidence required in PR。

| ID | 建議標題／標籤 | 依賴 | 完成證據 |
| --- | --- | --- | --- |
| R-001 | 鎖定 Core/pool 來源、授權與 algorithm facts · `research` | 無 | SHA、檔案清單、license、來源連結、TBD |
| R-002 | 建立完整 32-byte KAT + negative test · `correctness` | R-001 | 固定 vectors、故意破壞末 byte 測試 fail |
| R-003 | Windows/Linux native harness + CI · `platform` | R-002 | 三個目標 OS 的原生 logs、10k parity |
| S-001 | Stratum dialect 與 difficulty/target fixtures · `protocol` | R-001 | 來源引用、byte-level golden fixtures |
| S-002 | Stratum mock pool state machine · `protocol` | S-001、R-003 | clean_jobs/reconnect/reject/duplicate tests |
| S-003 | CPU CLI 真實 accepted-share interoperability · `integration` | S-002 | pool response、session logs、soak report |
| A-001 | Versioned config + secret redaction · `security` | S-002 | schema、migration、redaction tests |
| A-002 | Readiness Check / CLI doctor · `ux` | S-003、A-001 | pass/warn/fail/unknown fixtures |
| A-003 | Share Doctor event taxonomy · `diagnostics` | S-003 | correlation ID、各 reject 類型 fixtures |
| A-004 | Application command/state layer · `architecture` | S-003 | CLI/GUI contract tests、stop/timeout tests |
| U-001 | Desktop first-run wizard + one-click · `gui` | A-002、A-004 | clean install、明確同意、start/stop E2E |
| U-002 | Dashboard + evidence export + i18n · `gui` | A-003、A-004 | screenshot、redaction、繁中/英文 |
| P-001 | GPU backend ADR / CUDA proof-of-concept · `gpu` | R-003、S-003 | per-device parity、實卡 accepted shares |
| P-002 | AMD backend/telemetry capability matrix · `gpu` | R-003、S-003 | 每 OS/卡型/driver 能力與缺口 |
| L-001 | Failover/recovery ladder · `reliability` | S-003 | 同模式保障、退避/冷卻/上限 tests |
| L-002 | Native installer/tray/service + release pipeline · `release` | U-001、U-002 | 安裝/解除安裝、checksum、SBOM |
| E-001 | Benchmark baseline + adaptive efficiency · `performance` | P-001 或 CPU 已驗證路徑 | 可重現測量、rollback、溫度安全測試 |
| F-001 | Fleet threat model + opt-in API · `security` | L-002 | authn/authz、撤銷、rate limit、審計 |

### D3. GitHub 專案治理與 CI/CD

- **分支策略**：`main` 保持可讀與可重現；功能由短期 branch + PR 進入。保護 main：required checks、review、禁止強推；依實際 repository 權限與方案設定。
- **Labels**：`research`、`blocked-research`、`correctness`、`protocol`、`gpu`、`platform`、`security`、`gui`、`reliability`、`release`、`performance`、`documentation`；加 `P0/P1/P2` 與 `M0...M9`。
- **CI matrix**：Windows 11 對應的 Windows runner、Ubuntu 22.04/24.04；compiler/debug/release；lint、unit、KAT、mock-Stratum、config schema、security scan、artifact hash。GitHub hosted runner 不能替代指定實體 GPU／驅動驗收。
- **測試分層**：單元（序列化/target/nonce）→ deterministic KAT → mock pool fault injection → native OS integration → live accepted-share（人工批准、受控工作量）→ GPU hardware/soak → packaging smoke test。
- **Release**：版本號與 changelog、支援矩陣、source SHA、第三方 notices、SBOM、checksums、可重現 build 說明、安裝/解除安裝測試、簽章（具備安全金鑰流程時）。不能用 CI 綠燈替代真實 accepted-share 證據。
- **門檻**：任一 hash mismatch、未解釋 share verification failure、私鑰要求、未授權自動挖礦、遠端 API 預設開放，均為 release blocker。
- **測量品質**：報告 hashrate 時必須提供硬體、driver、OS、compiler、config、duration、樣本數、功耗來源與 variance；效能改善不可跨不同條件比較。

### D4. AI Coding Agent 長期工作契約

**每輪固定步驟：**
1. 讀取最新 `main`、`README.md`、`docs/ROADMAP.md`、`docs/G0_TECHNICAL_VALIDATION.md`、本計畫、`AGENTS.md`（若存在）、未結 Issue 與 ADR。
2. 找出**下一個尚未驗收的最小可交付**；只挑一個邊界清楚的 Issue，先列出依賴、已知事實、未知事項與驗收指令。
3. 若缺少演算法常數、Stratum field、difficulty formula、授權或 GPU 行為，停止該項猜測，建立 `blocked-research` 與來源需求，不把假設寫成 production code。
4. 小步提交：測試或 fixture → 實作 → Windows/Linux CI → 證據 → PR。變更若影響共識、API、設定 schema、GPU backend，先更新 ADR。
5. PR 必須附測試指令與輸出、平台／硬體、限制、失敗案例、授權差異、性能數據（適用時）。沒有設備或真實 pool 權限，狀態標 `pending`，不得寫 `passed`。
6. 每輪最後更新 Issue、研究紀錄與 roadmap 狀態；避免重複研究同一問題或反覆替換技術棧。

**建議新增的持久化文件：**
- `AGENTS.md`：專案目標、不可逾越安全界線、命令、測試門檻、Issue/PR 規範。
- `docs/RESEARCH_LEDGER.md`：每個外部技術事實記錄來源 URL、commit、日期、confidence、受影響模組、重查觸發條件。
- `docs/ADR/`：GPU backend、GUI framework、telemetry provider、config format、API transport、updater 的決策、替代方案與回退策略。
- `ASSUMPTIONS.md`：`PROVISIONAL / VERIFIED / REJECTED` 三態；任何狀態升級都要證據。
- `docs/VALIDATION_EVIDENCE/`：KAT、原生 build logs、mock pool、accepted-share、GPU/driver matrix、24h soak 與 release checksums。

**需人工批准的 gate**：真實 pool 長時間挖礦、實卡 GPU 支援聲明、thermal/power/clock tuning、遠端寫入 API、installer/background/autostart、release signing、更新機制。純 parser/unit/mock/config/docs/CI 可高度自動化。

### D5. 階段驗收記錄模板

```text
Issue:
Commit SHA / branch:
Target milestone:
Source references + pinned SHAs:
OS / architecture / compiler / driver / device:
Test commands:
Expected result:
Actual result (PASS / FAIL / PENDING):
Raw log / fixture / screenshot / pool response location:
Known limitations and blocked research:
Human validation / approval:
```

## E. 近期工作與開放問題

**下一個實際工程交付：G0/M0–M1，不是 GUI 或 GPU dashboard。** 先固定 Core 與 pool 的最新參考 SHA、授權與完整 32-byte known-answer tests，再建立 Windows/Linux 原生 harness。歷史評估指出 pool 的 `verify_known_vector.c` 曾只比對前 8 bytes 且 mismatch 仍回傳 0；需先重新核對當前來源，不能以其 exit 0 作為 parity 證據。

開放問題：
1. 現行 PEPEPOW Core 對 HooHash V110 variant 的完整 byte-level reference、header80/nonce/seed/cache、FP64 與 openlibm 語義為何？
2. 現行 PEPEPOW pool 的 Stratum dialect、difficulty baseline、target endian、clean_jobs、submit response 與 pool-based SOLO endpoint 的實際規格為何？
3. 各第三方來源是否允許原始碼重用、靜態連結、二進位再散布與簽章發行？
4. Windows 原生 GPU 與 Linux GPU 的 driver/API 支援矩陣、AMD telemetry 的實際可用性為何？
5. PEPEPOW 地址 checksum、pool/network 資料來源與收益估計 API 是否有可維護的正式規格？
6. GUI framework、服務管理、local API transport、資料庫與 updater，應在何種測試／維護成本條件下選定？

任何未回答問題均保留 **[TBD]**；不得以「應該」、「大概」作為 release acceptance。

## 參考來源與查核入口

以下供後續 Issue/ADR 逐項鎖定版本與重查；競品功能隨版本變動，不等同本專案已驗證支援。

- 本 repo：<https://github.com/edisontw/pepepow-miner>；`docs/G0_TECHNICAL_VALIDATION.md`；`docs/ROADMAP.md`
- PEPEPOW 挖礦入口：<https://pepepow.net/mining/>；Lab Pool：<https://pool.pepepow.net/>
- PEPEPOW Core：<https://github.com/MattF42/PePe-core>；pool 參考：<https://github.com/edisontw/pool-pepepow>
- HTN Miner：<https://htn.foztor.net/>；Hoominer：<https://github.com/HoosatNetwork/hoominer>
- XMRig miner/API/benchmark：<https://xmrig.com/docs/miner/>、<https://xmrig.com/docs/miner/api>、<https://xmrig.com/docs/miner/benchmark>
- GMiner：<https://github.com/develsoftware/GMinerRelease>；lolMiner：<https://github.com/Lolliedieb/lolMiner-releases>
- NVIDIA NVML：<https://docs.nvidia.com/deploy/nvml-api/latest/index.html>
- AMD SMI：<https://rocm.docs.amd.com/projects/amdsmi/en/latest/>；WSL 實驗性路徑：<https://rocm.docs.amd.com/projects/amdsmi/en/develop/how-to/amdsmi-wsl-mode.html>
- GitHub Actions matrix：<https://docs.github.com/en/actions/how-tos/write-workflows/choose-what-workflows-do/run-job-variations>
- GitHub rulesets：<https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/available-rules-for-rulesets>
- OpenAI Codex 工作流程：<https://openai.com/business/guides-and-resources/how-openai-uses-codex/>

---

**文件維護規則：**新增功能、改變優先序或升級技術假設時，必須連同來源、ADR、Issue 與驗收門檻更新；只有真實通過的證據才能將 milestone 狀態改成 completed。