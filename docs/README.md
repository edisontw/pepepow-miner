# 文件索引與下一步工作入口

> 更新：2026-11-04。此索引以 GitHub `main` 分支實際可讀的檔案為準；研究報告與規劃文件不等於已完成程式或驗證。

## 核心文件

| 文件 | 用途 | 狀態 |
| --- | --- | --- |
| [專案首頁](../README.md) | 產品定位、目前狀態、平台與安全界線 | 已存在 |
| [開發路線](ROADMAP.md) | G0–G5 階段、先後依賴與驗收門檻；**進度的主要依據** | 已存在 |
| [G0 技術驗證計畫](G0_TECHNICAL_VALIDATION.md) | CPU HooHash V110、完整 hash parity、原生建置與證據要求 | 已存在 |
| [03 產品開發計畫](research/03_PRODUCT_DEVELOPMENT_PLAN.md) | 競品、差異化、架構候選、UX、細部里程碑、AI Agent 規範 | 已存在 |
| [AI Coding Agent 指引](../AGENTS.md) | 每輪開發的閱讀順序、證據標準與安全規則 | 已存在 |

## 深度研究報告

三份編號文件集中於 `docs/research/`；保留原始檔名以利交叉引用。

| 編號 | 預定路徑 | GitHub `main` 查核狀態 |
| --- | --- | --- |
| 01 核心技術研究 | `docs/research/01_CORE_TECHNICAL_RESEARCH.md` | **待確認：目前未找到原始檔** |
| 02 架構與效能研究 | `docs/research/02_ARCHITECTURE_PERFORMANCE.md` | **待確認：目前未找到原始檔** |
| 03 產品開發計畫 | [`docs/research/03_PRODUCT_DEVELOPMENT_PLAN.md`](research/03_PRODUCT_DEVELOPMENT_PLAN.md) | 已移入、原文完整保留 |

01、02 據使用者回報已手動上傳，但目前連接的 GitHub `main` 分支未顯示這兩個檔案。**請先核對實際儲存庫、分支及上傳是否完成**。找到原始檔後應直接搬移原文，不建立假內容、不覆寫、不以推測補完。若已位於預定路徑，只需更新本索引狀態。

## 目錄慣例

```text
/
├── README.md                         # 專案入口
├── AGENTS.md                         # AI Coding Agent 操作規範
└── docs/
    ├── README.md                     # 本索引
    ├── ROADMAP.md                    # 唯一里程碑狀態入口
    ├── G0_TECHNICAL_VALIDATION.md    # 第一個技術驗收關卡
    └── research/
        ├── 01_CORE_TECHNICAL_RESEARCH.md   # 預定；尚未在 main 確認
        ├── 02_ARCHITECTURE_PERFORMANCE.md  # 預定；尚未在 main 確認
        └── 03_PRODUCT_DEVELOPMENT_PLAN.md  # 已存在
```

此圖包含「預定」項目，**不是目前已存在檔案的完整清單**。未來 ADR、研究來源帳本、驗收證據等目錄，等到有實際內容時再建立，避免空架構。

## 下一步：G0 的第一個可驗證交付

在進行 Stratum、GUI、GPU 或效能最佳化之前：

1. 確認 01／02 原始研究檔可讀，逐項標出「已驗證」、「待驗證」及可能與 [G0 計畫](G0_TECHNICAL_VALIDATION.md) 衝突的假設。
2. 重新查核 PEPEPOW Core 與 pool 的現行 commit、HooHash V110 實作路徑、依賴與授權；歷史 SHA 不視為已鎖定。
3. 建立**獨立來源**的 80-byte header／完整 32-byte expected digest 已知答案測例（KAT）；不得由待測實作在測試時產生 expected 值。
4. 建立原生 CPU CLI 測試 harness，完整比對 32 bytes，遇到任何 mismatch 或錯誤輸入必須回傳非零；刻意破壞最後一個 digest byte 必須測試失敗。
5. 再擴充到至少 10,000 組測例，並收集 Windows 11 x64、Ubuntu 22.04／24.04 x64 **原生**建置與執行證據。

**現況：G0 尚未通過。** 只建立文件、測試框架、編譯成功或單一測例通過，都不能將 G0 標為完成。完成門檻以 [G0 技術驗證計畫](G0_TECHNICAL_VALIDATION.md) 和 [Roadmap](ROADMAP.md) 為準。
