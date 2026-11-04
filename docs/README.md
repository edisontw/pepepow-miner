# PEPEPOW Miner 文件索引

> 更新：2026-11-04。以 GitHub `main` 的實際內容為準。**研究結論不等於已經實機驗證；規劃文件不等於已完成程式。**

## 文件位置與閱讀順序

| 文件 | 用途 | 狀態 |
| --- | --- | --- |
| [README](../README.md) | 產品定位與目前狀態 | 已存在 |
| [01 核心技術研究](research/01_CORE_TECHNICAL_RESEARCH.md) | PEPEPOW HooHash V110、header、Stratum、difficulty、授權與 golden vectors | 已上傳、可讀 |
| [02 架構與效能研究](research/02_ARCHITECTURE_PERFORMANCE.md) | CPU/GPU 架構、FP64 風險、Qt/C++ 方案、benchmark、CI | 已上傳、可讀 |
| [03 產品開發計畫](research/03_PRODUCT_DEVELOPMENT_PLAN.md) | 競品、差異化、UX、細部里程碑、工程流程 | 已歸檔、可讀 |
| [架構決策](ARCHITECTURE.md) | 研究衝突裁決、層次契約與條件式選型 | 規劃已採納、未實作 |
| [研究查核表](RESEARCH_LEDGER.md) | 固定 main/Core/pool SHA、逐項證據與未決問題 | 靜態查核；runtime pending |
| [G0 技術驗證計畫](G0_TECHNICAL_VALIDATION.md) | 首個技術驗收的範圍、測例與完成門檻 | 待執行 |
| [Roadmap](ROADMAP.md) | **G0–G5 階段進度與完成門檻的主要依據** | G0 待執行 |
| [AGENTS.md](../AGENTS.md) | AI Coding Agent 的讀取順序、安全與證據規範 | 已存在 |

三份研究報告都位於 `docs/research/`，檔名與正文保留；僅將原先無法在 GitHub 使用的 `sandbox:/mnt/data/` 下載連結改為儲存庫相對連結。**研究原文內的 `cite…` 標記不是 GitHub 可解析的引用**，因此其「已驗證」字樣仍須以原始 Core/pool、版本 SHA、授權與可重現測試重新核對，不應直接升級成工程驗收事實。

## 目前文件樹

```text
/
├── README.md
├── AGENTS.md
└── docs/
    ├── README.md
    ├── ARCHITECTURE.md
    ├── RESEARCH_LEDGER.md
    ├── ROADMAP.md
    ├── G0_TECHNICAL_VALIDATION.md
    └── research/
        ├── 01_CORE_TECHNICAL_RESEARCH.md
        ├── 02_ARCHITECTURE_PERFORMANCE.md
        └── 03_PRODUCT_DEVELOPMENT_PLAN.md
```

## 研究整合後的執行規則

已完整讀取三份研究並核對 main；原文不改寫。技術選型以 [架構決策](ARCHITECTURE.md) 為準，事實與來源以 [查核表](RESEARCH_LEDGER.md) 為準，進度只在 [Roadmap](ROADMAP.md) 維護。

已採納 C/C++ + CMake 的 CPU-first 路線；Qt 為 G2 有條件首選，Rust/Tauri 為替代；AMD 不預設 HIP。明確區分 32-bit wire nonce／內部 allocator、job generation／submit eligibility，GPU 每個 candidate 都 CPU 複核。API/service、自動活動偵測與 SQLite 不擴大首版。

Core/pool 分支 SHA 已重新查核，但來源授權、runtime parity 與實際互通仍未完成。pool KAT 工具的 8-byte 比對／失敗 exit 0 缺陷已由當前來源確認；difficulty float rounding 另列 G1 邊界驗收。

## 下一步：G0-01

[**GitHub Issue #1：查核 Core/pool 來源與授權，建立首組完整 32-byte KAT**](https://github.com/edisontw/pepepow-miner/issues/1)

1. 閱讀 01／02／03，將與共識相關的研究主張逐項對照 Core/pool 最新 commit、原始碼與授權；記錄證據與待確認事項。
2. 取得獨立 reference 的 **80-byte header → 完整 32-byte expected digest**；記錄來源與 byte order。
3. 建立 CPU CLI harness，對全部 32 bytes 比對，非法輸入與 mismatch 回傳非零；故意改壞 digest 最後一個 byte 必須失敗。
4. 擴充至至少 **10,000** 組 parity，並收集 Windows 11 x64、Ubuntu 22.04／24.04 x64 **原生** build/run 證據，才能依 [G0 計畫](G0_TECHNICAL_VALIDATION.md) 評估是否通過。

**目前狀態：只有研究與規劃文件；尚無已驗收的礦工程式、hash parity、accepted shares 或發行檔。**
