# PEPEPOW HooHash V110 新一代挖礦軟體：架構、GPU 最佳化與品質保證研究

## 核心結論

PEPEPOW 已於主網高度 **4,354,200** 切換至 HooHash V110；更重要的是，PEPEPOW Core v2.9.0.4 曾因 Windows x64 的 FP64 可重現性問題，改用 openlibm 並採取更嚴格的 FP64 編譯設定。這代表新一代 miner 的第一級風險並不是 GUI、Stratum 或 CUDA API 本身，而是 **CPU/GPU、Windows/Linux、不同 compiler 與 math library 之間能否產生 bit-exact 的 consensus hash**。citeturn108182search0turn509647search0

Hoosat 的 reference implementation 顯示，HooHash 的主要計算包含 64×64 `double` matrix、64-element nibble vector、BLAKE3，以及依累積狀態選擇的 `sin`、`cos`、`exp`、`sqrt`、`floor` 等 FP64 非線性函式；因此它不是可以直接交給 GEMM/Tensor Core 的傳統矩陣乘法問題。citeturn475792view0turn475792view2turn475792view3turn475792view1

最推薦的產品架構是：

**C++20 Mining Core + CUDA NVIDIA Backend + HIP AMD Backend + OpenCL 相容性 Backend + Qt 6 GUI + CMake/Ninja。**

CLI、GUI、Windows Service/Linux daemon 全部共用同一 Mining Core；GPU backend 僅負責裝置、kernel、buffer 與 nonce batch，不直接持有 Stratum connection。CMake 現行版本原生支援 CUDA、HIP 與 C++ language，因此單一 build tree 管理三者是可行路線。citeturn854171search23turn854171search40

Qt 6 對 Windows/Linux 有成熟原生支援，Windows 也提供 `windeployqt` 處理 runtime 與 plugin deployment。Tauri 2 雖然可跨平台且適合作為替代 GUI，但 Windows 使用 WebView2，而且若核心仍以 C++/CUDA/HIP 為主，會增加 Rust↔C ABI↔C++/GPU 的維護邊界；Rust 官方互通文件也建議 Rust 與 C/C++ 之間採 C ABI。citeturn854171search0turn854171search1turn854171search16turn854171search9

本研究的完整 Markdown 交付物已建立：

[在 GitHub 閱讀本文件](02_ARCHITECTURE_PERFORMANCE.md)

## 架構設計

建議核心依賴關係維持下列方向，避免 GUI、network 與 kernel 彼此耦合：

```text
 CLI ─────────┐
 Qt GUI ──────┼──> Application Controller
 Service ─────┘            │
                           ├── Configuration
                           ├── Monitoring / Local API
                           ├── Stratum Client
                           │       │
                           │       └── Job / Difficulty / Share
                           │
                           └── Mining Scheduler
                                   │
                             HooHash V110 Core
                         ┌─────────┼──────────┐
                         │         │          │
                       CPU       CUDA        HIP
                                    \          \
                                     \        OpenCL fallback
                                      │
                              Candidate Nonces
                                      │
                            CPU Canonical Validator
                                      │
                                 Share Submit
```

多 GPU 應採「**一張卡一個 worker + 全域 job generation + 64-bit nonce allocator**」。收到新 Stratum job 時增加 generation；舊 GPU batch 即使完成，只要 generation 已失效就不得 submit。這樣可以在不讓 GPU backend 理解 Stratum protocol 的情況下處理 stale work、failover 與多卡 nonce 去重。

GPU 回傳的 candidate nonce 應在送往 pool 前，由 canonical CPU implementation **重新計算完整 HooHash**。這項成本只發生在極少數 candidate 上，卻能阻止 GPU compiler、driver 或 kernel regression 產生錯誤 share。

CPU/GPU 混合挖礦應支援，但不應預設 CPU 全核心開滿。CPU thread 還承擔 GPU driver submission、Stratum、GUI、telemetry 與 candidate validation；是否啟用 CPU mining應以「總 Hash/Watt 與 GPU throughput 是否下降」實測決定，而不是單純把 CPU hash rate 加到總數。

現有 Hoominer README 已有 CPU、CUDA、OpenCL、GPU selection 與 work multiplier 等概念；PEPEPOW 官方目前也列出 CPU、NVIDIA GPU、AMD GPU 套件，因此多 backend 是符合現況而非純理論設計。citeturn700156search6turn700156search1

## GPU 與效能策略

HooHash V110 很可能首先受 **FP64 throughput、雙精度 transcendental latency、branch divergence 與 register pressure** 限制，而不是顯存頻寬。64×64 double matrix 約為 32 KiB，而且同一 job 可供大量 nonce 重用；相對地，內層迴圈可能反覆執行昂貴的 FP64 非線性運算。這是根據 reference code 的結構推論，必須再由 Nsight Compute、rocprof 等 profiler 驗證。citeturn475792view2turn475792view3

NVIDIA consumer GPU 尤其值得注意。CUDA 最新 Best Practices throughput 表中，compute capability **8.6/8.9 與 12.0/12.1 的 FP64 add/mul/FMA throughput 為每 SM 每 clock 2 results**，相較之下 CC 8.0 是 32、CC 9.0 是 64。RTX 4090 為 CC 8.9，而 RTX 5090 為 CC 12.0，因此不能以 FP32 TFLOPS、CUDA core 或 AI TOPS 預估 HooHash 表現。citeturn363956search8turn363956search0turn363956search3turn363956search2

RTX 4090 的官方 TGP 為 450 W，RTX 5090 為 575 W，因此比較時必須同時報告 **Hash/s 與 Hash/Watt**，否則可能得出與實際礦機效率完全不同的結論。citeturn363956search3turn363956search2

AMD 建議採 HIP primary、OpenCL fallback。AMD 目前 Windows HIP/ROCm 支援已涵蓋 RX 9000、RX 7900 與部分 RDNA2，但不同型號有 runtime-only 與完整 HIP SDK 支援差異；Linux 支援矩陣也不是所有歷代 GPU 都涵蓋。citeturn363956search5turn363956search6 OpenCL 因此仍有兼容價值，但 FP64 是 optional capability，啟動時必須檢查 `cl_khr_fp64` 或相應 OpenCL 3.0 feature。citeturn322250search18

最佳化優先序應是：

1. job-invariant hoisting、matrix/read-only data placement、candidate-only readback；
2. nonce batch 與 launch overhead 最佳化；
3. block/workgroup、register、spill、occupancy sweep；
4. branch divergence 與 nonlinear path profiling；
5. BLAKE3 kernel/profile；
6. architecture-specific Ada/Blackwell/RDNA kernels；
7. 最後才考慮會改變 FP evaluation order 的最佳化。

BLAKE3 的官方 C implementation 已包含 runtime CPU SIMD dispatch，因此 CPU backend 應先重用成熟實作，而非重新發明 SIMD hash；但 GPU 上 BLAKE3 是否值得優先重寫，仍應依 profile 中的時間比例判斷。citeturn316010search5turn316010search20

最需要禁止的是未驗證的 `fast-math`。NVCC 的 `--use_fast_math` 會改變精度相關設定，NVCC 也可能做 FMA contraction；NVIDIA 同時指出 host CPU 與 GPU 使用不同 math library，double `sin` 等函式是以 ULP 誤差界定，而不是承諾不同平台 bit-identical。AMD 的 `-ffast-math` 也被官方描述為允許 aggressive、lossy floating-point optimizations。citeturn322250search0turn322250search2turn322250search27

因此任何「快 5%、10%、20%」的 kernel 改動，在 **Golden Vector、CPU/GPU differential test、實際 share validation** 全部通過以前，都只能稱為待驗證假設。

## Benchmark 與品質保證

Golden Vector 應保存的不只是最終 digest，而是必要時連同 matrix digest、第一輪 BLAKE3、timestamp、nonce 與 source version，讓 regression 能快速定位是 hash、FP nonlinear stage、endianness 還是 target comparison 出錯。

正式 correctness gate 應為：

```text
PEPEPOW Core / canonical oracle
            ==
       CPU backend
            ==
       CUDA backend
            ==
        HIP backend
            ==
      OpenCL backend
```

比較的是 **完整 32-byte digest bit-for-bit 相同**，不能用浮點 epsilon。PEPEPOW Core 曾特別修正 Windows x64 的 FP64 determinism，正說明這種 gate 是必要條件。citeturn509647search0

Golden corpus 至少應覆蓋 `nonce & 0xff` 的所有 256 個值、不同 timestamp、各 nonlinear branch 邊界、NaN/Inf recovery 路徑、endianness，以及已知 mainnet block headers。

Stratum Mock Server 應模擬 subscribe、authorize、difficulty、job、accepted/rejected share、extranonce change、斷線、TLS error、malformed JSON、duplicate share、pool failover 與新 job 在 GPU batch 中途抵達等情況。現有 Hoominer 已提供 TCP 與 TLS/SSL Stratum 選項，因此新 miner 至少應維持此等級的 transport 能力。citeturn562502view2

競品 benchmark 要固定同一 GPU、同一 driver、同一 OS、同一 power limit、同一溫度狀態與同一 job replay stream。至少記錄：

```text
raw Hash/s
accepted shares
rejected / stale shares
board power
wall power（如有）
Hash/Watt
temperature
clock
GPU reset/error
process memory / VRAM
```

不能拿兩個 pool 的 dashboard hash rate 直接比較，也不能只引用 miner 自報的最高瞬時數字。

建議 release candidate 經過 **24 小時 functional soak** 與 **72 小時 final soak**。正式版最重要的穩定性標準是：miner 自身造成的 invalid hash/share 為 0、多 GPU nonce 不重疊、GPU reset/OOM 能隔離或乾淨恢復、斷線後不提交舊 generation、記憶體不持續成長。

## Build、CI/CD 與安全

Build 建議採 CMake presets：

```text
windows-msvc-cpu
windows-msvc-cuda
windows-msvc-hip
linux-gcc-cpu
linux-gcc-cuda
linux-clang-hip
linux-opencl
```

CLI、GUI 與 daemon 都連結相同 `libpepepow_miner_core`。Qt 僅存在於 GUI target，headless miner 不需要攜帶 Qt dependency。CMake 對 CUDA/HIP 的原生 language support 可直接支援這種配置。citeturn854171search23

Windows 建議提供 signed installer 與 CLI portable package；Qt runtime 可以 `windeployqt` 部署。citeturn854171search1 Windows release binary/package 應使用 SHA-256 Authenticode 並 timestamp，Microsoft SignTool 文件亦明確建議 SHA-256；若使用 MSIX，其 package signing 可再提供完整性保護。citeturn316010search4turn316010search6

Linux 則提供 tarball、`.deb`/`.rpm` 與 systemd service。GUI package 可以另外提供 AppImage，但 daemon 與 mining core 不應依賴桌面 sandbox。

CI 應區分一般 hosted runner 與實體 GPU self-hosted runner。GitHub Actions 可以透過 matrix 做 Windows/Linux build，也能用類似 `self-hosted, linux, x64, gpu` 的 labels 指定 GPU runner。citeturn316010search39turn316010search53

Release pipeline 建議依序：

```text
source tag
→ deterministic toolchain
→ unit / golden / integration
→ GPU differential tests
→ package
→ SHA256
→ code/package signing
→ SBOM
→ artifact attestation
→ publish
```

GitHub Artifact Attestations 使用 Sigstore 機制產生 build provenance；它應作為 Authenticode/manifest signature 的補充，而不是取代正式 binary signing。citeturn854171search37 Dependency Review 則可在 PR 階段阻止新增已知危險 dependency。citeturn316010search24turn316010search44

## 風險與實作優先順序

最大的技術風險依序是 **FP64/libm deterministic behavior、consumer GPU 的低 FP64 throughput、transcendental branch divergence、AMD Windows/Linux 支援差異、OpenCL driver 差異，以及 GPU toolchain 升級改變浮點行為**。其中 correctness regression 的嚴重度最高，因為錯誤可能表面上仍有很高 hash rate，實際卻產生無效工作。citeturn322250search2turn363956search8turn363956search5

另外，Hoosat `hoohash` repository 目前存在值得人工確認的授權訊號：repository LICENSE 檔案是 MIT，但 `hoohash.c` 原始碼檔頭仍包含 GPLv3-or-later 文字。正式產品若直接複製 source code，應先釐清該矛盾；較低風險方式是依公開演算法行為與 test vectors 重新實作 production kernel。citeturn364164view0turn834742view0

使用者指定的 `edisontw/pepepow-miner` 在本次研究時公開 URL 回傳 404，因此本研究無法對它進行程式碼層 audit；目前的架構判斷主要以 PEPEPOW Core、Hoosat HooHash/Hoominer、PEPEPOW mining 資訊，以及 NVIDIA、AMD、Qt、CMake 等第一方技術文件為基礎。citeturn562502view0turn108182search0turn509647search4

推薦的實作順序是 **Correctness-first CPU oracle → CUDA 單卡 → Golden/Cross Validation → Stratum mock → CUDA profiling → multi-GPU → 24h soak → HIP → OpenCL fallback → Qt GUI/API/service → signed updater → 72h release gate**。這樣可以先消除 HooHash V110 最危險的 correctness 風險，再投入 GUI 與產品化工程，而不會出現「miner 看起來很快，但結果不能被 consensus 接受」的情況。

完整的架構圖、Repository directory tree、Backend interface、benchmark matrix、CI/CD、正式驗收門檻與風險矩陣收錄於交付文件：

[在 GitHub 閱讀本文件](02_ARCHITECTURE_PERFORMANCE.md)