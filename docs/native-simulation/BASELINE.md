# Baseline and reproduction receipt

Date: 2026-10-02. Native Windows x64. Source and asset identities below are verified
locally; build and launch outcomes are recorded separately. No gameplay timing
source is changed by this architecture pass.

## Checkout identity

| Item | Value |
|---|---|
| Workspace | `C:\Users\Chopin\Documents\ChatGPT\Harkinian` |
| Fork / origin | `https://github.com/ChocoChopin/Shipwright.git` |
| Upstream | `https://github.com/HarbourMasters/Shipwright.git` |
| Experimental branch | `mod/native-simulation-rates` |
| Upstream develop base | `9eafd15fe1382c5a41e881f1b6ea87345c797d18` |
| libultraship | `62e973aeb4a53ad4d22bb91e2d9373ecdfcd246c` |
| torch | `2ab12fe9660aec04e02ee89fe81baed304a1a1d6` |
| Source project version | `9.2.3` |

GitHub CLI authorization permitted creating the fork. `gh repo view` confirms it
is a fork of HarbourMasters/Shipwright. The supplied directory already contained
an empty Git repository; it was populated by fetching full upstream develop history,
then the fork's develop/tags, instead of nesting an unrelated clone. Submodules
were initialized recursively at the source-pinned SHAs; no pointers changed.

Commands used:

```powershell
gh repo fork HarbourMasters/Shipwright --clone=false
git remote add origin https://github.com/ChocoChopin/Shipwright.git
git remote add upstream https://github.com/HarbourMasters/Shipwright.git
git fetch upstream develop
git switch -c mod/native-simulation-rates upstream/develop
git submodule update --init --recursive
```

A later all-tags fetch encountered a missing **historical** libultraship object
`554078d082ee722ed51c196f59828de762ed2a9b` while following old tags. The current
source-pinned submodule is present and clean. For tag metadata use
`git fetch --no-recurse-submodules origin --tags`; initialize only the selected
checkout's submodules. This historical-fetch issue does not invalidate the current
checkout or justify changing its submodule identity.

## Toolchain

| Tool | Observed value |
|---|---|
| Git | `2.47.1.windows.2` |
| GitHub CLI | `2.101.0` |
| CMake | `4.4.3` (project minimum 3.26) |
| Generator | `Visual Studio 17 2022`, platform x64, toolset v143 |
| VS Build Tools | `17.14.37516.0` / 17.14.37 |
| MSVC compiler | `19.44.35228.0`; tool directory `14.44.35207` |
| Windows SDK selected by CMake | `10.0.26100.0` |
| Python used for CMake/tools | `C:\Users\Chopin\AppData\Local\Programs\Python\Python312\python.exe`, 3.12.5 |
| Other installed Python | 3.14 and 3.10; not selected for the baseline |
| Ninja available | 1.13.2; baseline uses VS generator |
| vcpkg checkout resolved | `3aea538b2bb21a586502c67b00eb474fdd2e3098` |
| vcpkg target triplet | `x64-windows-static` |

`cl` and MSBuild need not be globally on PATH: the VS CMake generator locates them.
`vswhere -all` alone returned no instances because BuildTools is a separate product;
`vswhere -products '*' -all -format json` found the complete installed toolchain.
No toolchain install, security-policy change, WSL or remote Windows session was needed.

The initial configure built 21 vcpkg packages in 8.2 minutes and completed in
696.5 seconds plus 2.4 seconds generation. Full log remains local at
`build/native-simulation-evidence/configure.log`. Missing POSIX feature probes on
Windows and a CMake compatibility warning in Prism were nonfatal probes/warnings.

Upstream's Automate-VCPKG helper follows the current vcpkg branch and runs `git pull`
on reconfigure; dependency drift is therefore possible even at a pinned Shipwright
SHA. Keep the captured vcpkg tree for this baseline. For a repeat build on a fresh
machine, clone vcpkg at the recorded revision under build/x64/vcpkg and detach it
before configure (the upstream helper's attempted pull then cannot update it).
Capture FetchContent revisions too; not all dependency identity comes from submodules.

| FetchContent source | Resolved Git SHA |
|---|---|
| dr_libs | `da35f9d6c7374a95353fd1df1d394d44ab66cf01` |
| imgui | `4806a1924ff6181180bf5e4b8b79ab4394118875` |
| libgfxd | `008f73dca8ebc9151b205959b17773a19c5bd0da` |
| monocypher | `0d85f98c9d9b0227e42cf795cb527dff372b40a4` |
| prism | `1de054450e7b3c5f777d2e3dfcb228ad120c329d` |
| stormlib | `28c9b4be3f23c6b3a5ff55cacac7dbe5b9cdc4fc` |
| threadpool | `097aa718f25d44315cadb80b407144ad455ee4f9` |
| tinyxml2 | `321ea883b7190d4e85cae5512a12e5eaa8f8731f` |
| yaml-cpp | `56e3bb550c91fd7005566f19c079cb7a503223cf` |
| zlib | `51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf` |

## ROM and asset safety

The existing local input was found at
`B:\ReComps\Legend of Zelda, The - Ocarina of Time (USA).z64`.
It is 33,554,432 bytes with SHA1
`ad69c91157f6705e8ab06c79fe08aad47bb57ba7`. This matches **NTSC 1.0 (US)** in
`docs/supportedHashes.json` and an extraction entry in `soh/assets/yml/config.yml`.
The original was not renamed, modified or removed. An identical ignored local copy
is `roms/oot-ntsc-us-10.z64`.

Existing root ignores cover `.z64/.n64/.v64`, `.otr/.o2r`, build outputs and saves;
`roms/.gitignore` excludes all inputs. `git check-ignore -v` verified ROM and archive
paths **before copying**. This pass adds only workspace-local disposable-directory
ignores. No ROM, extracted asset payload, game archive, runtime save or binary is
committed. Ignore rules prevent ordinary accidental adds; Git `-f` can override
them, so explicit staging and staged-content review remain mandatory.

Current extraction is **Torch/O2R**, not an obsolete external ZAPD workflow. The
`GenerateSohOtr` target name is retained but generates `soh.o2r`; `ExtractAssets`
builds `soh-torch`, uses the ignored roms folder and generates `oot.o2r`. The chosen
vanilla ROM does not produce `oot-mq.o2r`. `ExtractAssetHeaders` is intentionally
unsupported upstream; use checked-in headers. See root CMakeLists.txt:288-360 and
[upstream Windows instructions](https://github.com/HarbourMasters/Shipwright/blob/9eafd15fe1382c5a41e881f1b6ea87345c797d18/docs/BUILDING.md).

## Repeatable commands

From the workspace, run the intended interpreter directly; no activation or
PowerShell execution-policy change is needed:

```powershell
& 'C:\Users\Chopin\AppData\Local\Programs\Python\Python312\python.exe' scripts/native-simulation/baseline.py verify-rom --rom roms/oot-ntsc-us-10.z64
& 'C:\Users\Chopin\AppData\Local\Programs\Python\Python312\python.exe' scripts/native-simulation/baseline.py configure
& 'C:\Users\Chopin\AppData\Local\Programs\Python\Python312\python.exe' scripts/native-simulation/baseline.py generate --config Release --jobs 4
& 'C:\Users\Chopin\AppData\Local\Programs\Python\Python312\python.exe' scripts/native-simulation/baseline.py extract --config Release --jobs 4
& 'C:\Users\Chopin\AppData\Local\Programs\Python\Python312\python.exe' scripts/native-simulation/baseline.py build --config Release --jobs 4
```

The configure wrapper expands to:

```powershell
cmake -S . -B build/x64 -G 'Visual Studio 17 2022' -T v143 -A x64 -DPython3_EXECUTABLE:FILEPATH=C:/Users/Chopin/AppData/Local/Programs/Python/Python312/python.exe
cmake --build build/x64 --config Release --parallel 4 --target GenerateSohOtr
cmake --build build/x64 --config Release --parallel 4 --target ExtractAssets
cmake --build build/x64 --config Release --parallel 4
```

Initial configure was run directly with process-local TEMP/TMP under `.tmp`.
Subsequent wrapper invocations additionally localize vcpkg root/download/binary
caches under `build/`, disable vcpkg metrics, record command/source/submodules/exit
code, and retain full logs. The initial upstream dependency bootstrap used vcpkg's
default binary-cache location; the durable wrapper avoids repeating that default.
No normal end-user application temp behavior is changed.

`--copy-rom` on the verification helper can create an ignored native-endian copy
from a supported input; it refuses to overwrite different bytes. Do not use it
to duplicate the already-copied input above. Run only one CMake operation in a
build directory at a time. If a command fails, retain its log and classify the
failure before modifying source. The helper does not install toolchains, fetch
ROMs, launch the game, or publish anything.

## Outcome receipt

Configure, `GenerateSohOtr`, and `ExtractAssets` completed successfully (exit 0).
The source HEAD for these operations was the exact upstream base above; the only
tracked worktree modification was `.gitignore`. New docs/scripts do not alter the
engine. The extraction itself reported 15.427 seconds. Local archives:

| Generated file | Bytes | SHA-256 |
|---|---:|---|
| `build/x64/soh/oot.o2r` | 33,569,565 | `a058767a2f4f8f415b099a5c5189a9bf974a068f331b88131afea8df24e6a997` |
| `build/x64/soh/soh.o2r` | 4,443,452 | `51c8b166b913fdc745901fc48a2ca6261e480e3e8c0715ae0f2d946adb982712` |

These are observed local package identities, not a promise that ZIP container
metadata makes future extraction byte-identical. No package contents are in Git.
The generated VS project places the Release executable at `x64/Release/soh.exe`,
outside `build/x64`; Torch tools are in `build/x64/Release`. All are ignored.

At this checkpoint executable compilation is in progress and launch is not yet
verified; this is not a claim of a completed baseline build.
The usage interruption did not damage extraction; its saved invocation receipt
confirms success. No background build was restarted or duplicated on resume.

### Local launch procedure

Portable Windows is the configured mode (`NON_PORTABLE=OFF`). It resolves archives
from the working directory, then executable directory; `SHIP_HOME` is not the
Windows isolation mechanism (`libultraship/src/ship/Context.cpp:539-609`). Use
`build/x64/soh` as the working directory to keep runtime config/logs/saves local.
Copy `build/x64/gamecontrollerdb.txt` there if available. The normal soh post-build
step supplies extractor assets next to the executable; preserve that directory.

```powershell
Start-Process -FilePath "$PWD\x64\Release\soh.exe" -WorkingDirectory "$PWD\build\x64\soh" -WindowStyle Hidden -PassThru
```

A bounded smoke run can record PID/path/start/exit/CPU and inspect
`build/x64/soh/logs/Ship of Harkinian.log`. Close only the process created for that
run. Process survival and startup/resource logs demonstrate initialization only;
they do not establish visual rendering, controller response or successful gameplay.
Do not invent an automatic test-mode/exit flag; one is not implemented upstream.
