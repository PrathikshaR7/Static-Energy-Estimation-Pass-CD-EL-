//===----------------------------------------------------------------------===//
// EnergyPass.cpp
//
// Out-of-tree LLVM IR Function Pass — Static Energy Estimator Skeleton
// Target: AArch64 (Apple M1 Pro), also compilable on Windows/x86-64
//
// Phase roadmap:
//   Phase 2 (this file) — classify IR instructions, placeholder costs, report
//   Phase 3             — extract real AArch64 opcodes from MIR via llc
//   Phase 4             — load energy costs from external JSON model
//   Phase 7             — weight estimates by block execution frequency
//   Phase 8             — emit results via LLVM Optimization Remarks
//===----------------------------------------------------------------------===//

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include <map>

using namespace llvm;

//===----------------------------------------------------------------------===//
// Instruction Category Taxonomy
//
// Coarse-grained buckets that map to distinct energy profiles on M1 AArch64.
// Each will correspond to a row in the JSON energy model (Phase 4).
//===----------------------------------------------------------------------===//
enum class InstrCategory {
  Integer,      // ADD, SUB, MUL, AND/OR/XOR, shifts, icmp
  FloatScalar,  // FADD, FSUB, FMUL, FDIV — scalar FP unit
  FloatVector,  // Vector/NEON: <4 x float> etc — wide SIMD on M1
  Memory,       // Load / Store / Atomic — dominates energy in real programs
  Branch,       // Br, Switch, Ret — pipeline flush cost
  Call,         // Direct and indirect calls — prologue/epilogue overhead
  Other         // PHI, alloca, GEP, bitcast — near-zero cost
};

static StringRef categoryName(InstrCategory C) {
  switch (C) {
    case InstrCategory::Integer:     return "Integer    ";
    case InstrCategory::FloatScalar: return "FloatScalar";
    case InstrCategory::FloatVector: return "FloatVector";
    case InstrCategory::Memory:      return "Memory     ";
    case InstrCategory::Branch:      return "Branch     ";
    case InstrCategory::Call:        return "Call       ";
    default:                         return "Other      ";
  }
}

//===----------------------------------------------------------------------===//
// Instruction -> Category Mapping
//===----------------------------------------------------------------------===//
static InstrCategory classify(const Instruction &I) {
  // Memory — unambiguous, check first
  if (isa<LoadInst>(I) || isa<StoreInst>(I) ||
      isa<AtomicRMWInst>(I) || isa<AtomicCmpXchgInst>(I))
    return InstrCategory::Memory;

  // Control flow
  if (isa<UncondBrInst>(I) || isa<CondBrInst>(I) || isa<SwitchInst>(I) ||
      isa<IndirectBrInst>(I) || isa<ReturnInst>(I))
    return InstrCategory::Branch;

  // Calls
  if (isa<CallInst>(I) || isa<InvokeInst>(I))
    return InstrCategory::Call;

  // FP and vector — determined by result type
  Type *Ty = I.getType();
  if (auto *VTy = dyn_cast<VectorType>(Ty)) {
    Type *Elem = VTy->getElementType();
    return Elem->isFloatingPointTy() ? InstrCategory::FloatVector
                                     : InstrCategory::Integer;
  }
  if (Ty->isFloatingPointTy())
    return InstrCategory::FloatScalar;

  // Integer and pointer arithmetic
  if (Ty->isIntegerTy() || Ty->isPointerTy())
    return InstrCategory::Integer;

  return InstrCategory::Other;
}

//===----------------------------------------------------------------------===//
// Placeholder Energy Costs (relative units — replaced by JSON in Phase 4)
//
// Rationale for AArch64/M1:
//   Memory (L1 ~4 cycles, L2 ~12, DRAM ~150+) >> FP > Integer
//===----------------------------------------------------------------------===//
static double placeholderCost(InstrCategory C) {
  switch (C) {
    case InstrCategory::Integer:     return 1.0;
    case InstrCategory::FloatScalar: return 2.5;
    case InstrCategory::FloatVector: return 3.5;
    case InstrCategory::Memory:      return 8.0;
    case InstrCategory::Branch:      return 1.5;
    case InstrCategory::Call:        return 5.0;
    default:                         return 0.5;
  }
}

//===----------------------------------------------------------------------===//
// EnergyPass
//===----------------------------------------------------------------------===//
namespace {

struct EnergyPass : public PassInfoMixin<EnergyPass> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    // Skip declarations (extern / intrinsic — no body to analyse)
    if (F.isDeclaration())
      return PreservedAnalyses::all();

    std::map<InstrCategory, unsigned> counts;
    double totalEnergy = 0.0;
    unsigned totalInstrs = 0;

    for (auto &BB : F)
      for (auto &I : BB) {
        InstrCategory cat = classify(I);
        counts[cat]++;
        totalEnergy += placeholderCost(cat);
        totalInstrs++;
      }

    // Print report to stderr so it doesn't interfere with IR output on stdout.
    // This will be replaced by LLVM Optimization Remarks in Phase 8.
    errs() << "\n[EnergyPass] " << F.getName()
           << "  basic-blocks=" << F.size() << "\n";
    errs() << "  Category       Count    Cost\n";
    errs() << "  ---------------------------------\n";
    for (auto &[cat, count] : counts)
      errs() << "  " << categoryName(cat)
             << "    " << count
             << "        " << (count * placeholderCost(cat)) << "\n";
    errs() << "  ---------------------------------\n";
    errs() << "  Total instrs : " << totalInstrs << "\n";
    errs() << "  Total energy : " << totalEnergy << " (relative units)\n";

    // Analysis-only pass — we never modify IR, so all analyses remain valid
    return PreservedAnalyses::all();
  }

  // Run even on functions marked optnone (e.g. compiled with -O0)
  static bool isRequired() { return true; }
};

} // anonymous namespace

//===----------------------------------------------------------------------===//
// Plugin Registration
//
// These two symbols are what opt looks for when loading a pass plugin.
// After loading, "energy-pass" becomes available to the -passes= flag.
//===----------------------------------------------------------------------===//
llvm::PassPluginLibraryInfo getEnergyPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "EnergyPass", LLVM_VERSION_STRING,
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, FunctionPassManager &FPM,
           ArrayRef<PassBuilder::PipelineElement>) -> bool {
          if (Name == "energy-pass") {
            FPM.addPass(EnergyPass());
            return true;
          }
          return false;
        });
    }};
}

// Weak linkage lets opt load this as a dynamic plugin
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getEnergyPassPluginInfo();
}
