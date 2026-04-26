# Energy Pass — LLVM Static Energy Estimator

An out-of-tree LLVM pass plugin that statically estimates instruction-level
energy cost for AArch64 programs, targeting the Apple M1 Pro.

## Repo Layout
Static-Energy-Estimation-Pass-CD-EL-/
├── CMakeLists.txt
├── CMakePresets.json               ← shared cross-platform build config
├── CMakeUserPresets.json.template  ← copy this, fill in YOUR path
├── lib/EnergyPass.cpp              ← pass source (all phases build here)
├── include/
├── test/hello.c
└── docs/
(local only, gitignored — each person builds these themselves)
├── CMakeUserPresets.json
├── llvm-project/
└── build/

## Project Plan
- [x] Phase 1:  Environment Setup & LLVM Build
- [x] Phase 2:  Machine Pass Skeleton
- [ ] Phase 3:  Instruction Extraction & Opcode Mapping
- [ ] Phase 4:  Energy Model (JSON + Data Collection)
- [ ] Phase 5:  JSON Integration in Pass
- [ ] Phase 6:  Basic Energy Calculation
- [ ] Phase 7:  Frequency Analysis
- [ ] Phase 8:  LLVM Remarks Integration
- [ ] Phase 9:  Test Case Development
- [ ] Phase 10: Visualization Script
- [ ] Phase 11: Evaluation & Comparison
- [ ] Phase 12: Scripts & Documentation

## Prerequisites

### macOS (Apple Silicon)
```bash
xcode-select --install
brew install cmake ninja git    # need cmake 3.23+
```

### Windows
1. Install Visual Studio 2022 — check **"Desktop development with C++"**
2. Install [Git for Windows](https://git-scm.com/download/win)
3. Install [CMake](https://cmake.org/download/) — add to PATH during install
4. Install [Ninja](https://github.com/ninja-build/ninja/releases) — extract
   `ninja.exe` to somewhere on your PATH (e.g. `C:\tools\`)
5. All build commands must be run in **"Developer Command Prompt for VS 2022"**

## First-Time Setup (do this once on each machine)

### Step 1 — Clone the repo
```bash
git clone https://github.com/PrathikshaR7/Static-Energy-Estimation-Pass-CD-EL-.git
cd Static-Energy-Estimation-Pass-CD-EL-
```

### Step 2 — Create your local preset file
```bash
cp CMakeUserPresets.json.template CMakeUserPresets.json
# Edit CMakeUserPresets.json and uncomment the section for your OS.
# The default path works if your LLVM build is at Static-Energy-Estimation-Pass-CD-EL-/build/.
```

### Step 3 — Clone LLVM (inside the repo, gitignored)
```bash
git clone --depth 1 https://github.com/llvm/llvm-project.git
cd llvm-project
git config --add remote.origin.fetch '^refs/heads/users/*'
git config --add remote.origin.fetch '^refs/heads/revert-*'
cd ..
```

### Step 4 — Build LLVM (~45-90 min)

**macOS:**
```bash
cmake -S llvm-project/llvm -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DLLVM_TARGETS_TO_BUILD="AArch64;X86" \
  -DLLVM_ENABLE_PROJECTS="clang" \
  -DLLVM_INCLUDE_TESTS=OFF \
  -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_PARALLEL_LINK_JOBS=2
cd build && ninja && cd ..
```

**Windows (Developer Command Prompt for VS 2022):**
\`\`\`
cmake -S llvm-project\llvm -B build -G Ninja ^
-DCMAKE_BUILD_TYPE=RelWithDebInfo ^
-DLLVM_TARGETS_TO_BUILD="AArch64;X86" ^
-DLLVM_ENABLE_PROJECTS="clang" ^
-DLLVM_INCLUDE_TESTS=OFF ^
-DLLVM_ENABLE_ASSERTIONS=ON ^
-DLLVM_PARALLEL_LINK_JOBS=2
cd build && ninja && cd ..
\`\`\`

### Step 5 — Build the pass plugin

**macOS:**
```bash
cmake --preset mac-arm64
cmake --build --preset mac-arm64
```

**Windows (Developer Command Prompt for VS 2022):**
\`\`\`
cmake --preset windows-msvc
cmake --build --preset windows-msvc
\`\`\`

## Running the Pass

**macOS:**
```bash
BUILD=~/Static-Energy-Estimation-Pass-CD-EL-/build
PASS=~/Static-Energy-Estimation-Pass-CD-EL-/pass-build/mac-arm64/lib/EnergyPass.dylib

$BUILD/bin/clang -O1 -emit-llvm \
  -isysroot $(xcrun --show-sdk-path) \
  test/hello.c -c -o /tmp/hello.bc

$BUILD/bin/opt -load-pass-plugin="$PASS" \
  -passes="energy-pass" -disable-output /tmp/hello.bc
```

**Windows (Developer Command Prompt):**
\`\`\`
set BUILD=C:\Users\YourName\Static-Energy-Estimation-Pass-CD-EL-\build
set PASS=C:\Users\YourName\Static-Energy-Estimation-Pass-CD-EL-\pass-build\windows-msvc\lib\EnergyPass.dll
%BUILD%\bin\clang -O1 -emit-llvm test\hello.c -c -o %TEMP%\hello.bc
%BUILD%\bin\opt -load-pass-plugin="%PASS%" ^
-passes="energy-pass" -disable-output %TEMP%\hello.bc
\`\`\`
