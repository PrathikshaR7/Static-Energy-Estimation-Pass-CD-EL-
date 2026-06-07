# EnergyPass — Phase 11: Evaluation & Comparison

Validation of the static energy model against published
AArch64 per-instruction energy data from academic literature.

## References

- [1] Tsoutsouras et al., "Energy Characterisation of Embedded
      Processors", IEEE TCAD 2013
- [2] ARM Cortex-A55 Software Optimization Guide, DDI0618, 2019
- [3] Retkowski & Stechele, "Energy-Efficient Instruction
      Scheduling for ARM", DATE 2017

------------------------------------------------------------------------
## 1. Model Validation — Cost Ranges vs Literature
------------------------------------------------------------------------

Each category cost compared against published range,
normalized to Integer=1.0.

Category         Ours   Ref Lo   Ref Hi Result  Reference
------------------------------------------------------------------------
Integer          1.00     1.00     1.00 [PASS]  Baseline — 1 cycle ALU ops [1,2]
FloatScalar      1.50     1.40     1.80 [PASS]  FP scalar: 1.4-1.8x integer [1,2,3]
FloatVector      2.50     2.00     3.50 [PASS]  FP vector/NEON: 2.0-3.5x [2,3]
Memory           2.00     1.80     4.00 [PASS]  Load/store L1: 1.8-4.0x (latency proxy) [1,2]
Branch           1.50     1.00     2.00 [PASS]  Branch: 1.0-2.0x (pipeline cost) [1,2]
Call             3.00     2.50     4.00 [PASS]  Call: 2.5-4.0x (prologue+epilogue) [1,3]
Other            0.10     0.05     0.20 [PASS]  PHI/alloca/metadata: near-zero [1]

Result: ALL PASS

------------------------------------------------------------------------
## 2. FP vs Integer Ratio Validation (Test 06)
------------------------------------------------------------------------

Weighted energy ratio of fp_sum_sq vs int_sum_sq.
Expected 1.0-2.5x from literature.

  int_sum_sq  weighted energy : 122.5000
  fp_sum_sq   weighted energy : 128.0000
  Observed ratio (FP/Int)     : 1.045x
  Expected range              : 1.0x – 2.5x
  Result                      : [PASS]

  Note: ratio reflects full function including integer address
  arithmetic overhead in both functions.

------------------------------------------------------------------------
## 3. Memory Dominance Validation
------------------------------------------------------------------------

Memory cost per instruction vs integer cost per instruction.
Expected ratio >= 1.8x from literature.

Function               Mem cnt  Mem/instr Int cnt  Int/instr   Ratio  Result
------------------------------------------------------------------------
matrix_add                   3     20.000       8      7.750    2.58x  [PASS]
matrix_transpose             2    200.000      18     37.000    5.41x  [PASS]
seq_sum                      1     20.000       6      7.000    2.86x  [PASS]
strided_sum                  1     20.000       7      6.143    3.26x  [PASS]
fill_array                   1     20.000       6      7.000    2.86x  [PASS]

Result: ALL PASS

------------------------------------------------------------------------
## 4. Loop Frequency Amplification Validation
------------------------------------------------------------------------

Weighted energy must exceed static by at least the minimum
amplification factor expected from loop nesting depth.

Function                 Static   Weighted     Amp  Min Exp  Result
------------------------------------------------------------------------
matmul                    47.50   26426.50   556.3x   100.0x  [PASS]
bubble_sort               36.00    1980.00    55.0x    20.0x  [PASS]
matrix_transpose          32.50    1265.50    38.9x    20.0x  [PASS]
gcd                       10.50      60.00     5.7x     3.0x  [PASS]
popcount                  12.50      80.00     6.4x     3.0x  [PASS]

Result: ALL PASS

------------------------------------------------------------------------
## 5. Known Limitations
------------------------------------------------------------------------

1. Static vs dynamic: the model counts instructions statically.
   Actual energy depends on runtime values (branch prediction,
   cache hit rates, DVFS state).

2. Memory model: flat 2.0 cost per load/store regardless of cache
   level. Real cost varies 2-50x (L1 vs DRAM).

3. Frequency proxy: 10^depth is a heuristic. Real trip counts
   depend on runtime inputs. Relative rankings are preserved.

4. SIMD/NEON: FloatVector category exists but is not exercised
   at -O1. Requires -O2 or explicit intrinsics.

5. Inlining: inlined functions appear as constituent instruction
   categories rather than Call.

------------------------------------------------------------------------
## 6. Summary
------------------------------------------------------------------------

- Model cost ranges        : consistent with published AArch64 data
- FP/Integer ratio         : within expected bounds from literature
- Memory cost per instr    : higher than integer, as expected
- Loop amplification       : weighted energy scales with depth
- Static analysis approach : validated as feasible for relative
                             energy ranking without hardware
