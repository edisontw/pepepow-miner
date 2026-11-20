# 開發路線與目前進度

更新：2026-11-04（台灣）

本次已完成三份研究整合、main 檔案盤點及部分 Core/pool 靜態來源查核，詳見 [架構決策](ARCHITECTURE.md) 與 [研究查核表](RESEARCH_LEDGER.md)。這不代表 G0 完成。

目前已有規劃、來源盤點與離線 C harness 前端；native Windows 的 synthetic 前端測試已通過，見 [實作紀錄](G0_IMPLEMENTATION_NOTES.md)。尚無真正 HooHash parity、可用礦工或 accepted share 證據。

| 階段 | 可驗證交付 | 完成門檻 | 狀態 |
| --- | --- | --- | --- |
| G0 技術驗證 | 來源鎖定、授權清單、CPU hash harness、測例與報告 | 至少 10,000 組完整 hash parity，Windows／Linux 原生執行 | 進行中：來源查核；未通過 |
| G1 CPU CLI | Stratum、job／target、nonce、重連、JSON 事件 | 雙平台實際 accepted shares，協定及故障 fixtures 通過 | 未開始 |
| G2 桌面 MVP | 開始／停止、手動資源 profile、診斷、設定、匣列、繁中／英文 | 全新環境安裝，睡眠／斷線恢復，各 OS 24h 觀察 | 未開始 |
| G3 Beta 發行 | 封裝、checksums、來源與授權、使用說明、支援矩陣 | 可重現 build，設定與診斷不洩漏敏感資料 | 未開始 |
| G4 NVIDIA | CUDA、監測、每 candidate CPU 複核 | Windows／Linux parity、實際 shares、24h 驅動觀察 | 未開始 |
| G5 AMD | OpenCL／HIP 路線與設備矩陣 | 各平台與設備分別驗收 | 未開始 |

## G1 要保留的協定驗收

- header80 的欄位與端序、Stratum prevhash 轉換、nonce／extranonce 範圍與避免重複。
- PEPEPOW difficulty baseline、target 邊界與等值比較；每個 job 保留其所屬 target。
- set_difficulty、clean_jobs、舊 job、重複 job、斷線與重連的生命週期。
- 所有回覆與拒收分類都保留來源；pool 回覆 accepted 不等同出塊或付款。
- 同模式備援池；不得因 POOL 斷線自動切換 SOLO 或改寫收款地址。
- 難度加權 accepted work 與本機算力分開，呈現樣本數及時間。
- 受控網路 invalid／low-diff 為 0；真實網路 stale 獨立記錄。
- 每個主要配置至少累積 1,000 筆已回覆 share 才評估拒收；低算力可用受控低難度環境。
- 實際挖礦由使用者明確啟動並設工作量界限；不把主網出塊當限時驗收條件。

## 首版邊界

包含原生 CPU、CLI／GUI、Stratum POOL 與 pool-based SOLO、收款地址檢查、手動資源模式、同模式重連／基本備援及事件式診斷。POOL/SOLO 每個 endpoint 分別取得互通證據；未驗證模式標為不可用。

G2 不要求自動活動偵測、數值健康評分、效率自調、收益追蹤、SQLite、HTTP API 或常駐 service；不因 03 的功能清單而擴大首版。

延後直接 wallet/RPC SOLO、GPU 超頻／電壓控制、自動交易、多幣、遠端寫入管理及雲端礦場控制。

## 下一批可執行工作包

以下是工作包 ID，不是假設已建立的 GitHub Issues。沿用 [Issue #1](https://github.com/edisontw/pepepow-miner/issues/1) 作第一個交付，不重複開單。

| 順序 / ID | 依賴 | 交付與驗收 | 狀態 |
| --- | --- | --- | --- |
| G0-01 / Issue #1 | 無 | 來源／授權 manifest、獨立完整 KAT、C/CMake harness、末 byte 負向測試非零退出；記錄實際命令 | [來源與 C 前端已交付](G0_IMPLEMENTATION_NOTES.md)；前端 debug/release 各 17 測例通過；Core KAT/整合 pending |
| G0-02 | G0-01 | 擴充 ≥10,000 corpus、低 8-bit nonce 全覆蓋、cache/FP64 診斷，Windows 11 + Ubuntu 22.04/24.04 原生報告 | 未開始 |
| G1-01 | G0 passed | 固定 dialect fixtures、header/target、獨立 mock、job/difficulty race、fragmented/multiple JSON frames、超長／畸形輸入、error shapes、TLS 失敗（適用時） | 未開始 |
| G1-02 | G1-01 | CPU CLI、有限重連／同模式 failover、JSONL 證據；明確授權與資源上限下各主要 OS/模式配置 ≥1,000 replied shares | 未開始 |
| G2-00 | G1 passed | Qt native 打包／授權 spike；失敗才以同條件評估 Tauri，提交選型證據 | 未開始 |
| G2-01 | G2-00 | 最小 wizard、start/stop、readiness、Share Doctor、匣列、診斷預覽、繁中／英文；三個目標 OS 各 24h | 未開始 |
| G3-01 | G2 passed | 可重現封裝、安裝／解除安裝、checksums、SBOM/notices、支援矩陣、敏感資料檢查；簽章狀態如實揭露 | 未開始 |
| G4-01 / G5-01 | G0/G1 passed；GPU 工具鏈與實卡 | CUDA 先單卡；AMD 路線 ADR；各 OS/device/driver 完整 parity、每 candidate CPU 複核、實際 shares、24h | 未開始 |

G1 target fixtures 必須處理 pool 的 float division/clamp 與研究 floor 公式的差異，並測 target−1、target、target+1；不得默認通用 Bitcoin baseline。Mock 不與待測 miner 共用 endian/target helper。每個 gate PR 附來源 SHA、命令、原始 logs、負向結果與 pending 限制。

G3 為 Beta：明確列出已驗證配置與缺口。02 的 **72h final soak** 留作正式穩定版門檻，按發行配置執行，並檢查 invalid=0、記憶體趨勢、故障恢復；24h 不冒充 72h。缺簽章能力不能宣稱 signed release。

## 執行原則

每輪先確認最新 main，讀取本 repo 的實際狀態；選擇下一個可驗證交付。完成必須附證據，區分 pending、failed、passed，不用計畫或 scaffold 冒充功能完成。

依序推進 G0 → G1 → G2 → G3；GPU 另設驗收。遇到可自行定位的技術問題繼續處理，只有缺少必要設備、外部存取或產品決策時記錄具體阻礙。

下一個交付請由 [G0 計畫](G0_TECHNICAL_VALIDATION.md) 的來源與完整 KAT 開始。
