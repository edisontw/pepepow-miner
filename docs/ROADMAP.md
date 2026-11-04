# 開發路線與目前進度

更新：2026-11-04（台灣）

目前交付只有產品規劃與技術驗收文件；沒有產品程式碼、編譯結果、hash parity 或實際 accepted share 證據。

| 階段 | 可驗證交付 | 完成門檻 | 狀態 |
| --- | --- | --- | --- |
| G0 技術驗證 | 來源鎖定、授權清單、CPU hash harness、測例與報告 | 至少 10,000 組完整 hash parity，Windows／Linux 原生執行 | 待執行 |
| G1 CPU CLI | Stratum、job／target、nonce、重連、JSON 事件 | 雙平台實際 accepted shares，協定及故障 fixtures 通過 | 未開始 |
| G2 桌面 MVP | 三步開始、日常模式、診斷、設定、匣列、繁中／英文 | 全新環境安裝，睡眠／斷線恢復，各 OS 24h 觀察 | 未開始 |
| G3 Beta 發行 | 封裝、checksums、來源與授權、使用說明、支援矩陣 | 可重現 build，設定與診斷不洩漏敏感資料 | 未開始 |
| G4 NVIDIA | CUDA、監測、CPU 抽樣複核 | Windows／Linux parity、實際 shares、24h 驅動觀察 | 未開始 |
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

包含原生 CPU、CLI／GUI、Stratum POOL 與 pool-based SOLO、收款地址檢查、資源模式、重連及診斷。

延後直接 wallet/RPC SOLO、GPU 超頻／電壓控制、自動交易、多幣、遠端寫入管理及雲端礦場控制。

## 執行原則

每輪先確認最新 main，讀取本 repo 的實際狀態；選擇下一個可驗證交付。完成必須附證據，區分 pending、failed、passed，不用計畫或 scaffold 冒充功能完成。

依序推進 G0 → G1 → G2 → G3；GPU 另設驗收。遇到可自行定位的技術問題繼續處理，只有缺少必要設備、外部存取或產品決策時記錄具體阻礙。

下一個交付請由 [G0 計畫](G0_TECHNICAL_VALIDATION.md) 的來源與完整 KAT 開始。
