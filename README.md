# Static Energy Estimation Pass

An out-of-tree LLVM analysis pass that statically estimates per-function
and per-block energy cost by combining a JSON-driven instruction energy
model with loop-depth frequency weighting, emitting results as LLVM
Optimization Remarks tied to source locations.

## Project Overview

Modern compilers have access to instruction mix and loop structure but
provide no energy feedback to developers. This pass bridges that gap by:

1. Classifying IR instructions into energy categories (Integer, Float,
   Memory, Branch, Call, etc.)
2. Loading per-category costs from a JSON energy model validated against
   published AArch64 literature
3. Weighting costs by loop nesting depth (10^depth proxy for execution
   frequency)
4. Emitting results as LLVM -pass-remarks-analysis=energy diagnostics
   tied to source file and line number
5. Generating a self-contained HTML report with annotated source code

## Deliverables

| # | Deliverable | Location |
|---|---|---|
| 1 | LLVM analysis pass | lib/EnergyPass.cpp |
| 2 | JSON energy model (AArch64 + x86-64) | data/energy_model.json |
| 3 | LLVM -Rpass-analysis=energy integration | lib/EnergyPass.cpp |
| 4 | HTML visualization with annotated source | scripts/visualize.py |
| 5 | Validation against published ARM data | scripts/evaluate.py, evaluation_results.md |

## Repository Layout

    Static-Energy-Estimation-Pass-CD-EL-/
    |-- CMakeLists.txt                  <- pass build config
    |-- CMakePresets.json               <- cross-platform presets (macOS + Windows)
    |-- CMakeUserPresets.json.template  <- copy this, fill in your LLVM path
    |-- data/
    |   |-- energy_model.json           <- AArch64 + x86-64 energy costs
    |-- lib/
    |   |-- CMakeLists.txt
    |   |-- EnergyPass.cpp              <- the pass (all phases)
    |-- test/
    |   |-- hello.c
    |   |-- 01_integer.c
    |   |-- 02_float.c
    |   |-- 03_memory.c
    |   |-- 04_nested_loops.c
    |   |-- 05_mixed.c
    |   |-- 06_fp_vs_int.c             <- FP vs integer ratio validation
    |   |-- 07_memory_latency.c        <- memory access pattern validation
    |-- scripts/
    |   |-- build.sh                   <- macOS: build pass plugin
    |   |-- build.bat                  <- Windows: build pass plugin
    |   |-- run_tests.sh               <- macOS: run full test suite
    |   |-- run_tests.bat              <- Windows: run full test suite
    |   |-- visualize.py               <- generate HTML report
    |   |-- evaluate.py                <- Phase 11 validation
    |-- evaluation_results.md          <- validation report
    |-- report.html                    <- generated HTML energy report
    |
    |   (gitignored - local only)
    |-- CMakeUserPresets.json
    |-- llvm-project/
    |-- build/

## Prerequisites

### macOS (Apple Silicon M1/M2/M3)

    xcode-select --install
    brew install cmake ninja git

CMake 3.23 or higher is required.

### Windows

1. Visual Studio 2022 - check "Desktop development with C++"
   https://visualstudio.microsoft.com/
2. Git for Windows
   https://git-scm.com/download/win
3. CMake - add to PATH during install
   https://cmake.org/download/
4. Ninja - put ninja.exe somewhere on your PATH
   https://github.com/ninja-build/ninja/releases
5. All commands must run in "Developer Command Prompt for VS 2022"

## First-Time Setup

### Step 1 - Clone the repo

    git clone https://github.com/YourUsername/Static-Energy-Estimation-Pass-CD-EL-.git
    cd Static-Energy-Estimation-Pass-CD-EL-

### Step 2 - Create your local preset file

macOS:

    cp CMakeUserPresets.json.template CMakeUserPresets.json

Windows:

    copy CMakeUserPresets.json.template CMakeUserPresets.json

Edit CMakeUserPresets.json and fill in the path to your LLVM build.
The default assumes build/ is inside the repo root which is correct
if you follow Step 4 below.

### Step 3 - Clone LLVM (inside repo, gitignored)

    git clone --depth 1 https://github.com/llvm/llvm-project.git
    cd llvm-project
    git config --add remote.origin.fetch '^refs/heads/users/*'
    git config --add remote.origin.fetch '^refs/heads/revert-*'
    cd ..

### Step 4 - Build LLVM (45-90 minutes)

macOS:

    cmake -S llvm-project/llvm -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DLLVM_TARGETS_TO_BUILD="AArch64;X86" \
      -DLLVM_ENABLE_PROJECTS="clang" \
      -DLLVM_INCLUDE_TESTS=OFF \
      -DLLVM_ENABLE_ASSERTIONS=ON \
      -DLLVM_PARALLEL_LINK_JOBS=2

    cd build && ninja && cd ..

Windows (Developer Command Prompt for VS 2022):

    cmake -S llvm-project\llvm -B build -G Ninja ^
      -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
      -DLLVM_TARGETS_TO_BUILD="AArch64;X86" ^
      -DLLVM_ENABLE_PROJECTS="clang" ^
      -DLLVM_INCLUDE_TESTS=OFF ^
      -DLLVM_ENABLE_ASSERTIONS=ON ^
      -DLLVM_PARALLEL_LINK_JOBS=2

    cd build && ninja && cd ..

### Step 5 - Build the pass plugin

macOS:

    ./scripts/build.sh

Windows:

    scripts\build.bat

## Running the Pass

### Quick single file

macOS:

    BUILD=./build
    PASS=./pass-build/mac-arm64/lib/EnergyPass.dylib

    $BUILD/bin/clang -O1 -g -emit-llvm \
      -isysroot $(xcrun --show-sdk-path) \
      test/hello.c -c -o /tmp/hello.bc

    $BUILD/bin/opt \
      -load-pass-plugin="$PASS" \
      -passes="energy-pass" \
      -pass-remarks-analysis=energy \
      -pass-remarks-output=/tmp/remarks.yml \
      -disable-output /tmp/hello.bc

Windows (Developer Command Prompt):

    set BUILD=.\build
    set PASS=.\pass-build\windows-msvc\lib\EnergyPass.dll

    %BUILD%\bin\clang.exe -O1 -g -emit-llvm ^
      test\hello.c -c -o %TEMP%\hello.bc

    %BUILD%\bin\opt.exe ^
      -load-pass-plugin="%PASS%" ^
      -passes=energy-pass ^
      -pass-remarks-analysis=energy ^
      -pass-remarks-output=%TEMP%\remarks.yml ^
      -disable-output %TEMP%\hello.bc

### Run full test suite

macOS:

    ./scripts/run_tests.sh

Windows:

    scripts\run_tests.bat

### Generate HTML report

    python3 scripts/visualize.py --all
    open report.html          # macOS
    start report.html         # Windows

### Run evaluation and validation

    python3 scripts/evaluate.py

## Energy Model

Located at data/energy_model.json. Costs are normalized relative
to Integer=1.0, validated against:

- ARM Cortex-A55 Software Optimization Guide (DDI0618)
  https://developer.arm.com/documentation/ddi0618/latest
- Tsoutsouras et al., "Energy Characterisation of Embedded Processors",
  IEEE TCAD 2013
- Retkowski and Stechele, "Energy-Efficient Instruction Scheduling
  for ARM", DATE 2017

| Category    | AArch64 | x86-64 |
|-------------|---------|--------|
| Integer     | 1.0     | 1.0    |
| FloatScalar | 1.5     | 1.8    |
| FloatVector | 2.5     | 3.0    |
| Memory      | 2.0     | 2.5    |
| Branch      | 1.5     | 2.0    |
| Call        | 3.0     | 3.5    |
| Other       | 0.1     | 0.1    |

## Frequency Weighting

Each basic block is weighted by 10^(loop_nesting_depth):

| Loop depth | Weight | Meaning              |
|------------|--------|----------------------|
| 0          | 1      | Straight-line code   |
| 1          | 10     | Single loop body     |
| 2          | 100    | Doubly-nested loop   |
| 3          | 1000   | Triply-nested loop   |

Loop depth is computed via back-edge detection on the CFG with no
LLVM analysis pass required, avoiding duplicate-symbol issues with
out-of-tree plugins on macOS.

## Known Limitations

1. Static model: counts instructions without runtime values. Branch
   prediction and cache behaviour are not modelled.

2. Flat memory cost: all loads/stores cost 2.0 regardless of cache
   level. Real L1/L2/DRAM costs differ by 10-50x.

3. Frequency proxy: 10^depth assumes approximately 10 iterations per
   loop level. Actual trip counts vary with input.

4. No SIMD at -O1: FloatVector category exists but clang does not
   auto-vectorize at -O1. Use -O2 or explicit intrinsics to exercise
   this category.

5. Inlining: functions inlined by clang appear as their constituent
   instruction categories rather than Call, which is correct behaviour
   but means the Call category is underrepresented in programs with
   many small functions.

## Project Plan

- [x] Phase 1:  Environment Setup and LLVM Build
- [x] Phase 2:  Machine Pass Skeleton
- [x] Phase 3:  Instruction Extraction and Opcode Mapping
- [x] Phase 4:  Energy Model (JSON + Data Collection)
- [x] Phase 5:  JSON Integration in Pass
- [x] Phase 6:  Basic Energy Calculation
- [x] Phase 7:  Frequency Analysis (loop-depth proxy)
- [x] Phase 8:  LLVM Remarks Integration
- [x] Phase 9:  Test Case Development
- [x] Phase 10: Visualization Script
- [x] Phase 11: Evaluation and Comparison
- [x] Phase 12: Scripts and Documentation
