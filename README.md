# PEPEPOW Miner

Windows／Linux 原生、日常電腦友善、能清楚解釋挖礦成效的 PEPEPOW 專用礦工。

**目前狀態：規劃與技術驗證準備。尚無可用挖礦程式或下載檔。**

## 產品方向

- 原生 Windows x64 與 Linux x64 CPU 核心，保留無介面 CLI。
- 日常使用模式：使用電腦時降載，閒置時恢復，提供清楚的開始／停止。
- 健康診斷：區分連線、授權、收到工作、accepted share 與拒收原因。
- 成效透明：分開呈現本機算力、難度加權工作量、礦池估算與資料時間。
- POOL／pool-based SOLO 明確分開，逐步整合出塊、成熟與付款追蹤。
- 繁體中文／英文介面，本機設定與可預覽的診斷匯出。

不宣稱上述功能各自首創；產品差異來自 PEPEPOW 專用整合、原生體驗與可驗證資料。

## 文件導覽

- [文件索引與下一步](docs/README.md)：研究文件位置、G0 驗證入口與目前可讀狀態。
- [產品開發計畫（03）](docs/research/03_PRODUCT_DEVELOPMENT_PLAN.md)：差異化、技術評估、里程碑與驗收。
- [AI Coding Agent 指引](AGENTS.md)：開發前必讀、G0 優先順序及證據規範。

## 第一個里程碑

先完成 [G0 技術驗證](docs/G0_TECHNICAL_VALIDATION.md)，確認 HooHash V110 與 PEPEPOW 共識實作一致，再開發 Stratum CPU CLI、桌面介面及 GPU backend。

通過 G0 需要完整 32-byte hash 比對、至少 10,000 組 header／nonce 測例、真實鏈上 header，以及 Windows／Linux 原生建置證據。建立測試框架或單一測例成功都不代表通過。

## 架構提案

| 層次 | 候選技術 | 狀態 |
| --- | --- | --- |
| 挖礦核心、Stratum、job／target 邏輯 | C/C++、CMake | 待 G0 驗證 |
| 本機管理、設定、程序生命週期 | Rust | 提案 |
| 桌面介面 | Tauri 2、TypeScript | 提案 |
| 本機歷史 | SQLite | 提案 |
| 引擎事件 | 版本化 JSON Lines | 提案 |
| NVIDIA／AMD | CUDA；OpenCL／HIP 評估 | 後續 |

三種語言的維護成本需要在 G0 結論中重新評估，架構尚未鎖死。Windows 原生挖礦不依賴 WSL；GUI 所需 WebView 等相依套件須由封裝處理。

## 開發路線與證據

請見 [Roadmap](docs/ROADMAP.md)。所有里程碑以實際測試證據標記，不能用預估進度代替驗收。

## 使用者信任

只輸入收款地址，不處理助記詞或私鑰。安裝後由使用者明確啟動挖礦；開機啟動及背景執行需可見、可關閉。初版不預設遙測或遠端控制。

自有核心零 devfee 為目前提案，尚未定案；正式發行須分別揭露 miner fee 與 pool fee。上游及依賴的授權需逐檔保留並完成檢視後，才能決定本專案的分發授權。

規劃基準：2026-11-04 可行性評估。外部專案版本、支援與效能均須在實作時重新查核。
