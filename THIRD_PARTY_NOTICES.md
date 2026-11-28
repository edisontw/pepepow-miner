# Distribution license compliance and third-party notices

**Updated: 2026-11-27.** This document tracks package-specific distribution
notices and provenance. It is not a legal opinion or a grant of rights. Its
open items do not block local development, offline tests, or G0 Technical
Validation. No source bundle or binary package has been released.

## Development baseline

- The Owner adopts the public Hoosat MIT `LICENSE` at commit
  [`9634f11410a2d71be21086e813263fa007fb6810`](https://github.com/HoosatNetwork/hoohash/commit/9634f11410a2d71be21086e813263fa007fb6810)
  together with the MIT HooHash file headers in the pinned PePe-core source as
  the current basis for continued local development and technical validation.
- That Hoosat commit changes the root `LICENSE` from GPLv3 to MIT and records
  the stated intent in its commit message. The older GPL-3.0-or-later headers
  on the upstream `hoohash.c` and `hoohash.h` remain part of the source
  history. Preserve the commit, headers, copyright notices, MIT license text,
  source SHA, checksums, and provenance; do not erase or rewrite those records.
- This development basis is not a determination that every third-party
  component in a future package is cleared for distribution. No additional
  written clarification about the HooHash transition is required before local
  coding, builds, or offline tests.

## Distribution release gate

- Before each source or binary release, generate the notices and SBOM for that
  exact release configuration. Include the applicable upstream license texts,
  attribution and copyright notices, compiled and linked components, bundled
  and required runtime libraries, source revisions, checksums, and patches.
- Keep any package-specific notice or provenance item that is not yet
  established marked pending for that package. Resolve it before distribution;
  this does not retroactively block development or technical acceptance.
- The project has no root `LICENSE`. Do not add a project-wide license as part
  of this dependency notice update.
- Owner approval governs project work and is not itself a third-party license
  grant. See the [Owner authorization](https://github.com/edisontw/pepepow-miner/issues/1#issuecomment-7192595038).

## Components and handling

| Component | Observed source or build evidence | Notice and distribution handling |
| --- | --- | --- |
| PEPEPOW Core | `COPYING` at [`bc08108ceccd957c79040dc7fedc1d56d8d47f97`](https://github.com/MattF42/PePe-core/tree/bc08108ceccd957c79040dc7fedc1d56d8d47f97); the Linux reference build compiled four Core crypto translation units. | Preserve the complete Core `COPYING`, each source header, and the source pin. The Core root MIT notice does not resolve notices of included or adapted components. |
| HooHash V110 | Pinned Core `hoohash.c` and `hoohash.h` have MIT headers and cite Hoosat commit `9634f11410a2d71be21086e813263fa007fb6810`. That upstream commit changes the root `LICENSE` from GPLv3 to MIT; the historical upstream file headers retain GPL-3.0-or-later wording. | Use the public MIT evidence as the current development basis. Preserve the MIT text, Core attribution, source pin, and historical GPL headers/transition provenance. For each release, include notices applicable to the exact distributed Core and HooHash files; this notice record does not claim that technical G0 acceptance clears other package components. Additional written clarification is not a prerequisite for local work. |
| BLAKE3 1.5.1 | The pinned Core build compiled `blake3.c`, `blake3_dispatch.c`, and `blake3_portable.c`; it included BLAKE3 headers. Core's copies contain reviewed changes relative to upstream. Upstream source is at [`54930c95227daaac4dcf1eb3028e2f4e0768d139`](https://github.com/BLAKE3-team/BLAKE3/tree/54930c95227daaac4dcf1eb3028e2f4e0768d139). | Preserve the upstream CC0-1.0 or Apache-2.0 license choice text, copyright/attribution notices, Core modifications, and patch/source provenance. The Core root MIT notice alone does not settle BLAKE3's terms. Keep distribution blocked pending review. |
| OpenLibm v0.8.7 | Pinned at [`9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d`](https://github.com/JuliaMath/openlibm/tree/9fbeafcd4f1b6ef6aa3946c1c8faead50f38a94d). The Linux Debug and Release builds each compiled 256 library translation units. The offline miner link map selects eleven OpenLibm members in Debug and ten in Release. | Retain the complete pinned `LICENSE.md` and notices for the exact compiled and linked source files. Do not assume that the root MIT text covers every file. Header-signal classifications in the inventories are evidence for review, not license determinations. Include additional components if a later target's link map selects them. |
| OpenSSL | This Linux build used Debian `libssl-dev` and `libssl3t64` version `3.5.7-1~deb13u3`; `libcrypto.so.3` is dynamically linked. Its Debian copyright file identifies Apache-2.0. | Record the exact OpenSSL package, runtime library, headers, and copyright text for each release target. Include the Apache-2.0 license and applicable notices if an OpenSSL runtime is bundled. OS-provided shared libraries must be listed as runtime requirements and included in the notice set if later redistributed with the package. |
| Pool-derived candidate/test material | The pinned `pool-pepepow` candidate is used for offline mapping/test evidence, not as compiled source in the Linux Debug or Release executables. The pinned pool repository has no root license or notice establishing redistribution terms for the candidate files. | Keep it marked `REVIEW_REQUIRED`; do not add it to release packages or source bundles until permission is documented or the fixture is replaced with independently generated material. |

## Actual Linux Debug and Release build records

The checked-in [Linux Debug dependency inventory](docs/sources/linux-debug-build-dependency-inventory.json) records 9 miner/Core translation units, 256 OpenLibm library translation units, and 802 unique compiler-traversed source/header inputs. The [Linux Release inventory](docs/sources/linux-reference-build-dependency-inventory.json) records 9 miner/Core translation units, 256 OpenLibm library translation units, and 804 unique compiler-traversed inputs. Both contain source and object hashes, compiler commands, static archive members and hashes, per-executable GNU linker maps, selected archive members, dynamic libraries, and Debian package copyright-file hashes. The source checkouts were clean at their pinned commits.

In both configurations, `offline_stratum_miner` links all four Core archive members. Debug links these eleven OpenLibm members: `e_exp.c.o`, `e_fmod.S.o`, `e_rem_pio2.c.o`, `e_sqrt.S.o`, `k_cos.c.o`, `k_rem_pio2.c.o`, `k_sin.c.o`, `s_cos.c.o`, `s_floor.c.o`, `s_scalbn.S.o`, and `s_sin.c.o`. Release links these ten: `e_exp.c.o`, `e_fmod.S.o`, `e_rem_pio2.c.o`, `k_cos.c.o`, `k_rem_pio2.c.o`, `k_sin.c.o`, `s_floor.c.o`, `s_scalbn.S.o`, `s_sin.c.o`, and `s_sincos.c.o`. The first-100-line header scan finds eight FDLIBM/Sun signals and three BSD-style signals among the Debug members, and eight FDLIBM/Sun and two BSD-style signals among Release members; these are notice-triage results, not license determinations. The miner dynamically uses OpenSSL `libcrypto.so.3`; each inventory records transitive runtime libraries and Debian package copyright files. These records describe Debian 13 x86-64 builds only and do not represent Windows 10/11 device testing.

The historical Windows Server 2022 OpenLibm artifacts contain compile databases but not compiler dependency lists or final linker maps. The updated workflow passed on PR #27 head `7fc8cfe7775bcc0a31f520b039b0ff45f519dd4c` in Windows Server 2022 UCRT64 Debug and Release and uploaded both dependency artifacts: [Debug artifact](https://github.com/edisontw/pepepow-miner/actions/runs/45600133198/artifacts/15015372150) (ZIP SHA-256 `8cb94b9f7a1115847b886f9d36dfca1fcdd1e13b5c310ffe7d72114491599842`) and [Release artifact](https://github.com/edisontw/pepepow-miner/actions/runs/45600133198/artifacts/15015885919) (ZIP SHA-256 `b144f88538595884ff195287da9c03ad635285dd55484ef46578c37def9f59ae`). They contain compile databases, Ninja compiler dependency paths, archive members, per-executable linker maps, imported DLL names, and binary hashes. Unlike the checked-in Linux inventories, the Windows artifacts are raw CI records and do not contain a SHA-256 for every compiler input. They are Windows Server CI evidence only, not Windows 10/11 physical-device testing or redistribution permission.

## Status

G0 Technical Validation is tracked separately in
[`docs/G0_TECHNICAL_VALIDATION.md`](docs/G0_TECHNICAL_VALIDATION.md). This
distribution gate remains pending until the applicable notices and SBOM are
complete for an exact source or binary package. The current project has no root
`LICENSE` and no redistributable package. A G0 Technical Validation PASS does
not mean that all third-party distribution requirements have been completed.
