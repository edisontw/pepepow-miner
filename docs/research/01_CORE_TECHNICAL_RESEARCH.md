# PEPEPOW 專用 Windows/Linux 挖礦程式核心技術研究

## 核心研究判定

**[已驗證事實]** PEPEPOW 主網的 HooHash V110 mandatory hard fork 啟用高度為 `4,354,200`。目前 Core 的 `src/chainparams.cpp` 設定 `consensus.nHoohashHeight = 4354200`；v2.9.0.2 release（commit `37d01de`）亦將此高度列為 HooHash V110 強制升級點。後續 v2.9.0.4（commit `c1394e6`）又針對 HooHash 的 FP64 一致性導入 openlibm 與更嚴格的浮點編譯條件，顯示跨平台浮點重現性是共識層問題，而非單純效能最佳化。citeturn533843view9turn928110search0turn839151search2

**[已驗證事實]** 實際 PoW dispatch 還依賴 block header 的 `nVersion`：`0x4000` 為 `HOOHASHV110_BIT`，`0x8000` 為 XelisV2；Core 的 `GetPOWHash()` 對 HooHash 路徑序列化完整 **80-byte block header** 後呼叫 `hoohashv110()`。因此工程上不應只以區塊高度硬切演算法，而應把 pool 給出的 version 正確寫入 header，並以 Core 行為作為最終 oracle。citeturn533843view8turn533843view9

**[已驗證事實]** PEPEPOW 的 HooHash V110 不能直接視為 upstream Hoosat HooHash 的 drop-in。PEPEPOW Core 的 `src/crypto/hoohash/hoohash.c` 明示它由 Hoosat `hoohash` commit `9634f11410a2d71be21086e813263fa007fb6810` 改編，而 PEPEPOW 現行規格明確使用 Bitcoin-derived 的 80-byte header、4-byte nonce 與 PEPEPOW 特定 matrix-seed 規則。citeturn184619view0turn325875view0

## HooHash V110 實作規格

**[已驗證事實]** PEPEPOW header 固定為 80 bytes：`nVersion` 位於 offset 0、prevhash 於 offset 4、merkle root 於 offset 36、`nTime` 於 offset 68、`nBits` 於 offset 72、`nNonce` 於 offset 76；整數欄位依 Bitcoin-derived header serialization 使用 little-endian。HooHash 函式若收到非 80-byte 輸入，現行 Core 直接輸出全 `0xff` 的失敗值，因此 miner 的 header builder 應把「恰好 80 bytes」列為不可繞過的 invariant。citeturn325875view0turn533843view5

**[已驗證事實]** 高階流程可規格化為：

```text
header80 = serialize(version, prevhash, merkle_root, ntime, nbits, nonce)

firstPass  = BLAKE3(header80)

seedHeader = header80
seedHeader[76:80] = 0x00000000
matrixSeed = BLAKE3(seedHeader)

matrix = GenerateMatrix(matrixSeed)          // 64 x 64 FP64
nonce  = LE32(header80[76:80])

mixed = MatrixTransform(
    matrix,
    firstPass,
    nonce,
    PEPEPOW_V110_constants
)

finalRaw = BLAKE3(mixed)
powHash  = reverse32(finalRaw)

return powHash
```

目前 Core 的 matrix PRNG 使用由 seed 建立的 xoshiro 狀態；matrix 為 `64×64`，亂數值經 `UINT32_MAX` 正規化並乘以 `1,000,000`。運算還包含 `sin`、`cos`、`exp`、`sqrt` 等 FP64 nonlinear transform，nonce 路徑使用 `nonce & 0xff`，並存在 `0.0001` divider、`1234` multiplier 等現行共識常數。這些細節都必須逐位元複製，不能用數學上「等價」但計算順序不同的寫法取代。citeturn107342view2turn107342view3turn107342view4turn107342view5

**[已驗證事實]** `SafeComplexTransform` 有一個尤其重要的共識行為：`transformedValue` 先被計算；若後續發現 NaN/Inf，程式會縮放輸入，但**沒有重新計算原先的 transformedValue**。從一般程式設計角度這可能看似可「修正」，但對礦工而言它是現行 consensus behavior，GPU kernel 或 CPU rewrite 都不得自行改寫語意。citeturn107342view4

**[推論]** 最大的 HooHash 工程風險是 FP64 的 determinism，而不是 BLAKE3。Core 的官方規格特別要求避免 fast-math、FMA contraction 與 excess precision，並在後續 release 導入 openlibm；因此 CUDA、OpenCL、HIP、MSVC 與 GCC/Clang 之間都應以逐階段 differential test 驗證，而不能只比較最終平均 hashrate。官方規格建議的 CPU 編譯限制包括 `-fno-fast-math`、`-ffp-contract=off`、`-fexcess-precision=standard`、`-mno-fma`、`-mno-fma4`，x86-64 還建議 SSE2 FP 路徑。citeturn325875view0turn928110search0

## Stratum、Difficulty 與 Share 相容規格

**[已驗證事實]** `pool-pepepow` 採典型 Stratum V1 JSON-RPC workflow，至少包含 `mining.subscribe`、`mining.authorize`、`mining.set_difficulty`、`mining.notify` 與 `mining.submit`。subscribe response 回傳 subscription 資訊、`extranonce1` 與 `extranonce2_size`；authorize 會把 `wallet.worker` 拆分；notify 的參數順序為 `job_id, prevhash, coinb1, coinb2, merkle_branch, version, nbits, ntime, clean_jobs`。citeturn184619view6turn107342view1

代表性的 wire flow 應實作為：

```json
{"id":1,"method":"mining.subscribe","params":[]}
{"id":2,"method":"mining.authorize","params":["WALLET.worker","x"]}

{"id":null,"method":"mining.set_difficulty","params":[DIFFICULTY]}

{"id":null,"method":"mining.notify","params":[
  "JOB_ID",
  "PREVHASH",
  "COINB1",
  "COINB2",
  ["MERKLE_BRANCH_0"],
  "VERSION",
  "NBITS",
  "NTIME",
  true
]}

{"id":3,"method":"mining.submit","params":[
  "WALLET.worker",
  "JOB_ID",
  "EXTRANONCE2",
  "NTIME",
  "NONCE"
]}
```

**[已驗證事實]** PEPEPOW pool 的 prevhash wire encoding 不是單純把完整 32 bytes reverse。`format_prevhash_for_stratum()` 將 64 個 hex 字元切成 8 個 4-byte words，再對每個 word 做 byte reversal。這是 miner job-builder 最容易產生「看起來合法、但所有 share 都錯」的 endian 陷阱之一。citeturn184619view6turn107342view1

**[已驗證事實]** 更關鍵的是 PEPEPOW pool 的 Stratum difficulty baseline。實際 pool source 定義：

```text
D1_PEPEPOW =
0000ffff00000000000000000000000000000000000000000000000000000000

share_target = floor(D1_PEPEPOW / difficulty)
```

同一來源明確註記，若錯用常見 Bitcoin Stratum diff-1 baseline，門檻會嚴格 **65,536 倍**；程式亦定義 `MANUAL_SHARE_DIFFICULTY_WIRE_SCALE = 65536.0`。因此 generic Bitcoin miner 的 difficulty helper 不應直接沿用。citeturn107342view0turn533843view1

**[已驗證事實]** 實際 pool 的 low-difficulty rejection 使用 Stratum error code `23` 與 `"Low difficulty share"`；pool 另有 current/previous/unknown/stale job 分類與 duplicate-window 邏輯。因此正式 miner 必須把 difficulty update、job swap、`clean_jobs`、submit queue 與 nonce scheduler 做成明確的 generation/state machine，而不是讓多張 GPU 直接共享可變 job struct。citeturn533843view2turn184619view9

## 現有礦工與授權比較

| 專案 | HooHash V110 / PEPEPOW | CPU | NVIDIA | AMD | Windows | Linux | 授權與工程價值 |
|---|---|---:|---:|---:|---:|---:|---|
| `edisontw/pepepow-miner` | 本次無法由公開 GitHub source 完成可信審查 | 未驗證 | 未驗證 | 未驗證 | 未驗證 | 未驗證 | **待取得 source snapshot**；不宜在無原始碼基礎下推定架構 |
| HTN Miner | PEPEPOW 實戰使用 | AVX2 CPU | CUDA | HIP/ROCm | 原生 Windows 非主要支援；可 WSL | 是 | 最適合作為**效能與 pool interoperability 黑箱 baseline**；目前公開頁面列有 `hoo_cpu`、`hoo_gpu`、`hoo_gpu_amd` citeturn632423view4 |
| Hoosat `hoominer` | 有 `pepepow` 模式 | 是 | PEPEPOW CUDA 標示 NO | OpenCL YES | 未形成可靠原生 Windows source base | 是 | Repo 明示 **closed source**；可作 protocol/結果比對，不能作可維護 fork base citeturn632423view1 |
| `MattF42/pepew-cpu-miner` | v4.4.2 加入 `hoohash-pepew` | 是 | 非主要方向 | 非主要方向 | 主要不是原生 Windows | 是 | GPLv2；適合作 CPU pipeline、Stratum 與 CLI 參考，直接整合會帶入 GPL 義務 citeturn562576search0turn562576search1 |
| `tpfuemp/ccminer-tpfuemp` | `hoohash` / `hoohashv110` / `pepepow` alias | 否 | CUDA | 否 | VS2022 + CUDA | 是 | 目前最有價值的開源 NVIDIA 實作參考；LICENSE 含 GPLv3 與第三方條款，直接 fork 應按 GPLv3-family 發佈模型規劃並做逐檔授權盤點 citeturn632423view0turn707509view2 |

**[已驗證事實／授權風險]** PEPEPOW Core 內的 `hoohash.c` 檔頭標示 MIT，但其註記又指出來源為 HoosatNetwork/hoohash 的特定 commit，而 upstream Hoosat repository 公開授權資訊為 GPLv3-or-later。這不代表可以直接下法律結論，但它形成了必須處理的 provenance 問題：若未做 commit-level lineage 與逐檔 license audit，不應假設「Core 裡看到 MIT header」就足以讓整個衍生實作自由採 permissive/proprietary license。citeturn184619view0turn562576search3

## Golden Test Vectors 與驗證策略

**[已驗證事實]** Core 官方 HooHash 規格本身就建議以固定 `hdr80 + expected digest` KAT 阻止錯誤 miner 啟動。對新 miner 而言，只驗最後 32-byte hash 還不夠，因為 endian、matrix seed、FP64 或最後 reverse 任一層都可能互相掩蓋問題。citeturn325875view0

**[推薦規格]** Golden Vector 套件應至少保存 `header80`、header 欄位解析值、`firstPass`、`matrixSeed`、選定 matrix cell 的 IEEE-754 binary64 bit pattern、nonce、matrix transform 中間輸出、`finalRaw`、byte-reversed `powHash`、`nBits → networkTarget`、指定 Stratum difficulty 的 `shareTarget`，以及 `powHash <= shareTarget` / `powHash <= networkTarget` 的期望布林結果。Vector 應覆蓋 nonce `0`、`1`、`255`、`256`、`0xffffffff`，並加入從真實主網 block、pool accepted share 與人工邊界案例擷取的資料。

**[研究缺口]** 本次可驗證的上游文件與 Core source 足以確定演算法流程，但沒有找到一組可直接作為正式 release KAT、同時具有穩定公開來源的「完整 80-byte header + 所有中間值 + 最終 digest」資料集。因此不應在規格內虛構 test vector。第一個工程 milestone 應是用當前 PePe-core reference binary 產生並凍結 versioned corpus，再由 CPU scalar、CUDA、OpenCL/HIP 各自做 differential validation。Core 的 80-byte input、matrix seed 與輸出 reversal 規則已可作為該 corpus 的權威生成條件。citeturn533843view5turn533843view6turn325875view0

Mock Pool 則應獨立於 miner 的 share validator 實作，測試 TCP fragmentation、多個 JSON frame 同 packet、`set_difficulty` 與 `notify` race、`clean_jobs=true`、previous-job grace window、duplicate share、low-difficulty share、disconnect/reconnect、TLS failure、pool failover，以及 submit response 延遲。若 mock server 與 miner 共用同一套 target 或 endian helper，會形成「兩邊同時錯卻測試通過」的 false positive。

## 主要技術風險與推薦方案

最高風險依序是：**FP64 跨 backend 不一致、誤用 upstream HooHash preimage、Stratum diff-1 相差 65,536 倍、prevhash/header endian 錯誤、clean-jobs race、GPU nonce range 重疊、以及 GPL/provenance 問題**。其中前四項都可能造成「礦工顯示有 hashrate，但 pool 幾乎 100% reject」的失敗模式；FP64 問題甚至可能只發生在特定 GPU 架構或 compiler flag 組合。相關 Core 與 pool source 已直接證明這些行為是 protocol/consensus-sensitive。citeturn325875view0turn107342view0turn107342view1turn928110search0

推薦正式架構不是直接 fork 單一現有礦工，而是：

```text
CLI / Config / Telemetry
          │
     Pool Manager
   reconnect / failover
          │
    Stratum V1 Codec
          │
   Immutable Job State
          │
 PEPEPOW Job Builder
 coinbase / merkle / endian
 header80 / targets
          │
 ┌────────┴───────────┐
 │                    │
CPU Reference      Work Scheduler
scalar oracle           │
                  ┌─────┼─────────┐
                  │     │         │
                AVX2   CUDA   OpenCL/HIP
                  │     │         │
                  └─────┴─────────┘
                        │
                Local Share Validator
                        │
                    Submit Queue
```

**[推薦]** 第一階段先完成完全對齊 PePe-core 的 scalar CPU reference、header builder、target/difficulty 與 Stratum client；第二階段才加入 CUDA，並要求每個 GPU nonce result 經獨立 CPU oracle 抽樣或全面複核；第三階段加入 AMD OpenCL/HIP；最後才做 AVX2、constant-memory matrix、nonce batching、kernel occupancy 等效能最佳化。這樣可以先把 consensus correctness 與 performance 分離，避免在 GPU kernel 內同時除錯 endian、FP64 與 Stratum。

若專案目標是**最快得到 NVIDIA Windows/Linux 可用版本，並可接受 GPLv3 發佈條件**，`ccminer-tpfuemp` 是最短的 fork 路徑，因為它已公開標示 PEPEPOW/HooHashV110 alias，且具 Windows CUDA 與 Linux build 基礎。citeturn632423view0turn707509view2

若目標是**長期控制 Windows/Linux、CPU/NVIDIA/AMD、多 pool 相容性與授權策略**，較穩健方案是 clean-room 建立 host、Stratum、job builder 與 GPU backends，把 **PePe-core 當 consensus oracle、`pool-pepepow` 當 Stratum oracle、現有 miners 當效能/interoperability baseline**。這能把未來 Core 共識變更、pool protocol 變更與不同 GPU backend 解耦，也較容易建立 CI KAT 與 backend differential testing。

## 完整交付檔

完整的工程級研究文件已整理為 `01_CORE_TECHNICAL_RESEARCH.md`，包含更完整的原始碼路徑、commit、演算法 pseudocode、Stratum message 定義、reuse/license 分析、Golden Vector schema、Mock Pool 測試矩陣、風險分級與建議實作順序：

[下載 01_CORE_TECHNICAL_RESEARCH.md](sandbox:/mnt/data/01_CORE_TECHNICAL_RESEARCH.md)