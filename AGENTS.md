# AI Coding Agent 指引 — PEPEPOW Miner

本文件適用整個 repository。所有變更以實際 `main`、可重現測試及使用者明確授權為準；不得把規劃文字當作已實作功能。

## 每輪開始先讀

1. `README.md` — 產品範圍與安全原則。
2. `docs/README.md` — 文件索引、已存在／待確認研究檔。
3. `docs/ROADMAP.md` — 唯一階段狀態來源。
4. `docs/G0_TECHNICAL_VALIDATION.md` — 當前第一關的具體驗收。
5. `docs/research/03_PRODUCT_DEVELOPMENT_PLAN.md` — 產品與工程規劃。
6. `docs/research/01_CORE_TECHNICAL_RESEARCH.md`、`docs/research/02_ARCHITECTURE_PERFORMANCE.md` — 兩份研究原文已上傳 `main`；開工前均須閱讀並逐項核對來源。

如有互相衝突的研究主張，先記錄原文、來源、commit、驗證方式與未決問題；不得直接據以修改共識演算法。研究文件不是協定權威，必須回到 Core/pool 的可追溯來源與測試。

02 研究的 C++20／Qt 6／CUDA／HIP 與 03／README 的 Rust／Tauri 候選均**尚未定案**；以 `docs/ROADMAP.md` 的 G0→G1→G2→G3 驗收順序為準。研究內 `cite…` 不是 GitHub 可解析引用，須補充可核對的來源 URL、commit、檔案路徑或實測證據。

## 當前工作關卡

**先做 G0，未驗收前不得宣稱礦工可用。** 下一個最小交付：

- 查核現行 Core/pool 來源 SHA、實作路徑、授權與可再散布條件。
- 取得獨立 expected digest，建立 80-byte header → 完整 32-byte hash 的第一組 KAT。
- 建立 mismatch／錯誤輸入會非零退出的原生 CPU harness；故意修改最後一個 digest byte 必須失敗。
- 逐步擴充到至少 10,000 組 parity 與 Windows 11 x64、Ubuntu 22.04/24.04 x64 原生證據。

單一 KAT、建立 scaffold、CI 綠燈但未執行平台測試，都不能將 G0 標記為 passed。G1 的 Stratum 與 accepted share、G2 的 GUI、G4/G5 的 GPU 均在各自 gate 後才可宣稱支援。

## 不可違反的界線

- 不要求、不記錄、不輸出私鑰、助記詞或錢包憑證；僅使用公開收款地址。
- 不在安裝、啟動或測試時暗中挖礦；真實 pool 測試須使用者明確批准並限制時間與資源。
- POOL 與 pool-based SOLO 分開；故障備援不得默默跨模式、改地址或改收費。
- 不預設遙測、遠端寫入 API、開機自動挖礦、超頻／電壓控制。
- 未驗證來源授權前不複製或發行第三方程式碼；保留 NOTICE、license、來源 SHA、patch 紀錄。
- 不杜撰測試結果、算力、GPU/OS 支援、accepted shares、出塊或收益。

## 工作與提交規範

- 每輪只選一個可驗證的最小交付，先讀現有檔案，再修改；避免無關重構。
- 每次變更列出：目的、受影響路徑、執行命令、測試結果（passed／failed／pending）、未解風險、下一步。
- 對外部依賴固定版本與校驗資訊；共識與端序的每項假設需附獨立參考證據。
- 失敗要保留可重現的輸入與非零退出；不能吞掉錯誤、只比 hash 前綴或讓 CI 假通過。
- 不在未取得實機證據前宣稱 Windows／Linux 原生測試通過；交叉編譯不能替代原生執行。
- 研究文件移動時保留原文與檔名，更新相對連結和 `docs/README.md`；找不到檔案應回報，不建立冒充原文的佔位內容。
- 有疑義的架構選擇先寫 ADR 或 Issue，明確區分提案與已採納決策。
