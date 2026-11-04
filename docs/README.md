# PEPEPOW Miner 文件索引

> 更新：2026-11-04。以 GitHub `main` 的實際內容為準。**研究結論不等於已經實機驗證；規劃文件不等於已完成程式。**

## 文件位置與閱讀順序

| 文件 | 用途 | 狀態 |
| --- | --- | --- |
| [README](../README.md) | 產品定位與目前狀態 | 已存在 |
| [01 核心技術研究](research/01_CORE_TECHNICAL_RESEARCH.md) | PEPEPOW HooHash V110、header、Stratum、difficulty、授權與 golden vectors | 已上傳、可讀 |
| [02 架構與效能研究](research/02_ARCHITECTURE_PERFORMANCE.md) | CPU/GPU 架構、FP64 風險、Qt/C++ 方案、benchmark、CI | 已上傳、可讀 |
| [03 產品開發計畫](research/03_PRODUCT_DEVELOPMENT_PLAN.md) | 競品、差異化、UX、細部里程碑、工程流程 | 已歸檔、可讀 |
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
    ├── ROADMAP.md
    ├── G0_TECHNICAL_VALIDATION.md
    └── research/
        ├── 01_CORE_TECHNICAL_RESEARCH.md
        ├── 02_ARCHITECTURE_PERFORMANCE.md
        └── 03_PRODUCT_DEVELOPMENT_PLAN.md
```

## 研究間需要釐清的選擇

| 議題 | 研究內容 | 現階段處置 |
| --- | --- | --- |
| GUI 與管理層 | 02 偏向 **C++20 + Qt 6**；03／README 將 **Rust + Tauri 2** 列為候選 | **未決策**。先完成 G0 CPU correctness，再以打包、維護成本、native Windows/Linux 和 UI 需求撰寫 ADR |
| GPU 實作順序 | 02 建議 correctness 後盡早 CUDA profiling；[Roadmap](ROADMAP.md) 定義 **G0 CPU → G1 Stratum CPU CLI → G2 GUI → G3 Beta**，GPU G4/G5 分開驗收 | 以 Roadmap 的交付關卡為準；GPU 可研究但不得繞過 correctness 或宣稱已支援 |
| HooHash／FP64／授權 | 01／02 提供特定來源、hard fork、header、openlibm 與授權風險的研究主張 | 先重新查核最新 Core/pool commit、實作、逐檔授權、KAT；不可直接引用研究文字當測試證據 |
| 測試向量 | 01 明確指出缺少可直接使用的完整正式 golden-vector corpus | **G0-01** 先建立獨立來源的第一組完整 32-byte KAT；不虛構測例 |

## 下一步：G0-01

[**GitHub Issue #1：查核 Core/pool 來源與授權，建立首組完整 32-byte KAT**](https://github.com/edisontw/pepepow-miner/issues/1)

1. 閱讀 01／02／03，將與共識相關的研究主張逐項對照 Core/pool 最新 commit、原始碼與授權；記錄證據與待確認事項。
2. 取得獨立 reference 的 **80-byte header → 完整 32-byte expected digest**；記錄來源與 byte order。
3. 建立 CPU CLI harness，對全部 32 bytes 比對，非法輸入與 mismatch 回傳非零；故意改壞 digest 最後一個 byte 必須失敗。
4. 擴充至至少 **10,000** 組 parity，並收集 Windows 11 x64、Ubuntu 22.04／24.04 x64 **原生** build/run 證據，才能依 [G0 計畫](G0_TECHNICAL_VALIDATION.md) 評估是否通過。

**目前狀態：只有研究與規劃文件；尚無已驗收的礦工程式、hash parity、accepted shares 或發行檔。**
