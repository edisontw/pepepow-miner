# PEPEPOW Miner

Windows／Linux 原生、日常電腦友善、能清楚解釋挖礦成效的 PEPEPOW 專用礦工。

**目前狀態：規劃與技術驗證準備。尚無可用挖礦程式或下載檔。**

## 產品方向

- 原生 Windows x64 與 Linux x64 CPU 核心，保留無介面 CLI。
- 日常使用模式：首版提供手動資源 profile 與清楚的開始／停止；自動活動偵測與調載延後。
- 健康診斷：區分連線、授權、收到工作、accepted share 與拒收原因。
- 成效透明：分開呈現本機算力、難度加權工作量、礦池估算與資料時間。
- POOL／pool-based SOLO 明確分開，逐步整合出塊、成熟與付款追蹤。
- 繁體中文／英文介面，本機設定與可預覽的診斷匯出。

不宣稱上述功能各自首創；產品差異來自 PEPEPOW 專用整合、原生體驗與可驗證資料。

## 文件導覽

- [文件索引與下一步](docs/README.md)：研究文件位置、G0 驗證入口與目前可讀狀態。
- [架構決策](docs/ARCHITECTURE.md)：核心、GUI、GPU 與工作生命週期的整合選擇。
- [研究查核表](docs/RESEARCH_LEDGER.md)：固定來源 SHA、已確認差異及待驗證項目。
- [產品開發計畫（03）](docs/research/03_PRODUCT_DEVELOPMENT_PLAN.md)：差異化、技術評估、里程碑與驗收。
- [AI Coding Agent 指引](AGENTS.md)：開發前必讀、G0 優先順序及證據規範。

目前已有 [離線 KAT 前端與測試紀錄](docs/G0_IMPLEMENTATION_NOTES.md)：synthetic 前端測試通過，尚未連結真正 Core reference，不可用來挖礦。

## 第一個里程碑

先完成 [G0 技術驗證](docs/G0_TECHNICAL_VALIDATION.md)，確認 HooHash V110 與 PEPEPOW 共識實作一致，再開發 Stratum CPU CLI、桌面介面及 GPU backend。

通過 G0 需要完整 32-byte hash 比對、至少 10,000 組 header／nonce 測例、真實鏈上 header，以及 Windows／Linux 原生建置證據。建立測試框架或單一測例成功都不代表通過。

## 架構決策

| 層次 | 決定 | 狀態 |
| --- | --- | --- |
| G0 hash harness | C reference adapter + CMake | 已採納規劃；授權與 parity 待驗證 |
| G1 host／Stratum／排程 | C++20；immutable job snapshots | 已採納規劃；未實作 |
| G2 桌面介面 | Qt 6 + C++，共用 controller；headless 不依賴 Qt | 有條件首選；打包／授權 spike 待驗證 |
| 設定與事件 | 版本化 JSON／JSONL | 已採納規劃；schema 待 G1 |
| NVIDIA／AMD | CUDA 先行；AMD 依實卡選 HIP 或 OpenCL | 獨立 G4/G5；未驗證 |
| Rust/Tauri、SQLite、service/API | 替代或後續需求 | 不列首輪必需依賴 |

依 [架構決策](docs/ARCHITECTURE.md) 執行；不把選型當作已支援。Windows 原生目標不依賴 WSL。

## 開發路線與證據

請見 [Roadmap](docs/ROADMAP.md)。所有里程碑以實際測試證據標記，不能用預估進度代替驗收。

## 使用者信任

只輸入收款地址，不處理助記詞或私鑰。安裝後由使用者明確啟動挖礦；開機啟動及背景執行需可見、可關閉。初版不預設遙測或遠端控制。

自有核心零 devfee 為目前提案，尚未定案；正式發行須分別揭露 miner fee 與 pool fee。上游及依賴的授權需逐檔保留並完成檢視後，才能決定本專案的分發授權。

規劃基準：2026-11-04 可行性評估。外部專案版本、支援與效能均須在實作時重新查核。
