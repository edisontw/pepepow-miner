# 研究查核與待驗證事項

查核日期：2026-11-04（台灣）。本次只讀取原始碼／文件，**未建置、執行 hash、連池或測量效能**。
VERIFIED-SOURCE 只代表靜態來源確認；PROVISIONAL 需測試；BLOCKED 表示該事項不得據此進入 production。沒有 runtime-passed 項目。

## 固定查核快照

- Miner main：[780d49568989415818f984e1141c06d465dc0dcb](https://github.com/edisontw/pepepow-miner/tree/780d49568989415818f984e1141c06d465dc0dcb)，8 份 Markdown，無產品程式、CI 或 LICENSE。
- Core master：[38e801428688dfd0aee272b4a91819ac1a13ab4e](https://github.com/MattF42/PePe-core/tree/38e801428688dfd0aee272b4a91819ac1a13ab4e)。
- Pool main：[c1298d7b40e5e9fd39f76af179c6e10a1c313147](https://github.com/edisontw/pool-pepepow/tree/c1298d7b40e5e9fd39f76af179c6e10a1c313147)。

Core/pool SHA 與原規劃歷史值相同，本次已重新讀取分支 head；它們是**查核快照，尚未引入或批准為可散布依賴**。

## 已讀的三份研究與處置

| 來源章節 | 建議或衝突 | 整合決定 |
| --- | --- | --- |
| [01：主要技術風險與推薦方案](research/01_CORE_TECHNICAL_RESEARCH.md) | CPU/Stratum 後 GPU；最快 NVIDIA 可 fork ccminer | 採自建 host + 經授權審查的 reference adapter；不以 GPU fork 作 MVP 基底 |
| [02：核心結論、風險與實作優先順序](research/02_ARCHITECTURE_PERFORMANCE.md) | C++/Qt、CUDA-first、HIP primary | C++20 host 採納；Qt 有條件首選；CPU CLI-first；AMD 路線延後實卡 ADR |
| [03：B3、D1、D2](research/03_PRODUCT_DEVELOPMENT_PLAN.md) | Rust/Tauri 候選、G0–G5/M0–M9 | 由架構文件統一選型；G0–G5 唯一狀態，不建立第二套進度 |
| 01 GPU 抽樣／全面複核 vs 02 每 candidate 複核 | submit 防線不一致 | 每個 GPU candidate 都 CPU 全長複核 |
| 01 4-byte nonce vs 02 64-bit allocator | wire 與內部排程混淆 | wire uint32；內部可 uint64，禁止 nonce wrap |
| 02 新 generation vs 01 previous-job grace | 過度丟棄或誤提交舊工作 | 分開 scheduler generation 與 job submit eligibility，按 pool fixtures 驗證 |
| 02 24h/72h vs 03 24h | 發行門檻混用 | 24h functional gate；72h 為穩定版 gate，Beta 明列尚未完成 |
| 03 P0/P1 清單 vs 原 README 日常模式 | 自動調載、API/service 擴大 MVP | G2 手動 profiles、事件診斷；自動活動偵測／遠端管理延後 |

## 來源證據與下一步

以下連結固定 SHA，查核以 symbol 為定位；來源或 toolchain 變更須重跑相關驗收。

| ID / 狀態 | 來源與觀察 | 工程處置／關卡 |
| --- | --- | --- |
| R01 VERIFIED-SOURCE | [Core spec](https://github.com/MattF42/PePe-core/blob/38e801428688dfd0aee272b4a91819ac1a13ab4e/doc/hoohash_spec.md)：80-byte header、nonce offset 76–79 LE、seed masking | G0 建 byte-level fixtures；不能拿 upstream Hoosat preimage 直接替代 |
| R02 VERIFIED-SOURCE | [block.cpp GetPOWHash](https://github.com/MattF42/PePe-core/blob/38e801428688dfd0aee272b4a91819ac1a13ab4e/src/primitives/block.cpp)：0x4000 HooHash 優先於 0x8000；序列化後 memcpy digest | G0 固定 version/header；非只按研究中的 fork 高度判斷；chainparams 的啟用規則仍須另核對 |
| R03 VERIFIED-SOURCE | [hoohash.c](https://github.com/MattF42/PePe-core/blob/38e801428688dfd0aee272b4a91819ac1a13ab4e/src/crypto/hoohash/hoohash.c)：hoohashv110 結尾已 reverse；HAVE_OPENLIBM 條件分支；非法長度填 0xff | G0 adapter 不重複 reverse；記錄實際編譯是否啟用 openlibm。block.cpp 非法長度路徑填零，證明不能把錯誤 digest 當有效答案；harness 一律拒絕非法輸入 |
| R04 VERIFIED-SOURCE / runtime pending | [dev_vectors.cpp](https://github.com/MattF42/PePe-core/blob/38e801428688dfd0aee272b4a91819ac1a13ab4e/src/crypto/hoohash/dev_vectors.cpp)：固定 seed 產生 100 組 header 並呼叫同一 hash 函式 | 可作 corpus 生成起點，不是獨立 expected corpus 或鏈上證明；註解的 byte-swap 描述須與實際 block.cpp 核對 |
| R05 VERIFIED-SOURCE | [verify_known_vector.c](https://github.com/edisontw/pool-pepepow/blob/c1298d7b40e5e9fd39f76af179c6e10a1c313147/apps/pool-core/tools/verify_known_vector.c)：有完整 header/digest 字串，但 memcmp 僅 8 bytes，FAIL 仍 return 0 | Issue #1 可查核該候選向量的 Core 來源與 encoding；不得直接承認為已通過 KAT。末 byte 負向測試必須非零退出 |
| R06 VERIFIED-SOURCE | [stratum_protocol.py](https://github.com/edisontw/pool-pepepow/blob/c1298d7b40e5e9fd39f76af179c6e10a1c313147/apps/pool-core/stratum_protocol.py)：format_prevhash_for_stratum 每 4-byte word reverse；notify/subscribe 結構；error 可 object 或 submit array | G1 固定完整 notify → header80 fixtures；測試兩類 error shape，不只 happy path |
| R07 VERIFIED-SOURCE / boundary pending | [stratum_ingress.py](https://github.com/edisontw/pool-pepepow/blob/c1298d7b40e5e9fd39f76af179c6e10a1c313147/apps/pool-core/stratum_ingress.py)：STRATUM_DIFF1_TARGET_PEPEPOW_POOL = 0000ffff…；_share_target_from_difficulty 用 int(D1 / difficulty) 後 clamp 至 [1, 2^256−1]，輸入為 float | 01 的 floor 公式是數學摘要，不足以定義浮點邊界。G1 比較精確整數／有理數 target 與 pool rounding，測試 target−1/target/target+1、極小／非有限 difficulty，未解差異阻擋互通驗收 |
| R08 VERIFIED-SOURCE | 同上 _manual_share_difficulty_from_password 的 sd= 路徑除以 65536；與 miner-facing target baseline 是不同層 | 不把 65536 再套到所有 set_difficulty；以設定→wire→target fixtures 核對 |
| R09 BLOCKED provenance | Core hoohash.c MIT 檔頭並註明 Hoosat commit 9634f11410a2d71be21086e813263fa007fb6810；01／02 對 upstream repo/file license 的敘述不同 | 尚未完成 lineage、BLAKE3/openlibm、第三方逐檔審查。G0-01 記錄 checksum、license、notice、patch 與相容性；不可由單一 LICENSE 或「重寫」推定可散布 |
| R10 PROVISIONAL | 02 的 FP64、GPU throughput、AMD Windows 能力與競品效能；03 的 GUI/telemetry 可行性 | 本 repo 無 hardware/build 證據。各 OS/device/driver 逐項驗證，不把研究數字當 benchmark |
| R11 PROVISIONAL | 01／02 的 SafeComplexTransform、常數、NaN/Inf、浮點順序等細節 | G0 逐 symbol 與 fixed Core 比對，中間值診斷；本次未完成全演算法 audit，禁止自行修正共識語義 |

## 證據產出規格

後續提交到 `docs/VALIDATION_EVIDENCE/<gate>/<run-id>/`（目前尚不存在）：source manifest、測例、實際重現命令、stdout/stderr、exit code、OS/CPU/compiler/flags、結果 passed/failed/pending、限制。不要先寫假的通過報告。
每條外部主張升級狀態要有 permalink 與測試證據；所有權責由對應 gate 的實作 PR 承擔。
