//===----------------------------------------------------------------------===//
// EnergyPass.cpp
//
// Phase 8: LLVM Remarks Integration via LLVMContext diagnostics
//
// Remarks are emitted directly through LLVMContext::diagnose() rather than
// OptimizationRemarkEmitterAnalysis, avoiding the duplicate-symbol problem
// with out-of-tree plugins on macOS.
//===----------------------------------------------------------------------===//

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#if __has_include(<llvm/Plugins/PassPlugin.h>)
#include "llvm/Plugins/PassPlugin.h"
#else
#include "llvm/Passes/PassPlugin.h"
#endif
#include "llvm/Support/JSON.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Triple.h"

#include <cmath>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace llvm;

//===----------------------------------------------------------------------===//
// Instruction Category Taxonomy
//===----------------------------------------------------------------------===//
enum class InstrCategory {
  Integer, FloatScalar, FloatVector, Memory, Branch, Call, Other
};

static const std::vector<InstrCategory> ALL_CATEGORIES = {
  InstrCategory::Integer,     InstrCategory::FloatScalar,
  InstrCategory::FloatVector, InstrCategory::Memory,
  InstrCategory::Branch,      InstrCategory::Call,
  InstrCategory::Other
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

static StringRef categoryKey(InstrCategory C) {
  switch (C) {
    case InstrCategory::Integer:     return "Integer";
    case InstrCategory::FloatScalar: return "FloatScalar";
    case InstrCategory::FloatVector: return "FloatVector";
    case InstrCategory::Memory:      return "Memory";
    case InstrCategory::Branch:      return "Branch";
    case InstrCategory::Call:        return "Call";
    default:                         return "Other";
  }
}

//===----------------------------------------------------------------------===//
// Instruction -> Category
//===----------------------------------------------------------------------===//
static InstrCategory classify(const Instruction &I) {
  if (isa<LoadInst>(I) || isa<StoreInst>(I) ||
      isa<AtomicRMWInst>(I) || isa<AtomicCmpXchgInst>(I))
    return InstrCategory::Memory;
  if (isa<BranchInst>(I) || isa<SwitchInst>(I) ||
      isa<IndirectBrInst>(I) || isa<ReturnInst>(I))
    return InstrCategory::Branch;
  if (isa<CallInst>(I) || isa<InvokeInst>(I))
    return InstrCategory::Call;
  Type *Ty = I.getType();
  if (auto *VTy = dyn_cast<VectorType>(Ty))
    return VTy->getElementType()->isFloatingPointTy()
               ? InstrCategory::FloatVector : InstrCategory::Integer;
  if (Ty->isFloatingPointTy())                return InstrCategory::FloatScalar;
  if (Ty->isIntegerTy() || Ty->isPointerTy()) return InstrCategory::Integer;
  return InstrCategory::Other;
}

//===----------------------------------------------------------------------===//
// Loop depth via back-edge detection (no FAM analysis needed)
//===----------------------------------------------------------------------===//
static std::map<const BasicBlock *, std::set<const BasicBlock *>>
computeDominators(const Function &F) {
  std::map<const BasicBlock *, std::set<const BasicBlock *>> dom;
  if (F.empty()) return dom;
  
  const BasicBlock *entry = &F.getEntryBlock();
  std::set<const BasicBlock *> allBlocks;
  for (auto &BB : F) allBlocks.insert(&BB);
  for (auto &BB : F)
    dom[&BB] = (&BB == entry) ? std::set<const BasicBlock *>{entry} : allBlocks;
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto &BB : F) {
      if (&BB == entry) continue;
      std::set<const BasicBlock *> newDom = allBlocks;
      for (const BasicBlock *pred : predecessors(&BB)) {
        std::set<const BasicBlock *> tmp;
        for (auto *b : newDom)
          if (dom[pred].count(b)) tmp.insert(b);
        newDom = tmp;
      }
      newDom.insert(&BB);
      if (newDom != dom[&BB]) { dom[&BB] = newDom; changed = true; }
    }
  }
  return dom;
}

static std::map<const BasicBlock *, unsigned>
computeLoopDepths(const Function &F) {
  std::map<const BasicBlock *, unsigned> depths;
  if (F.empty()) return depths;

  auto dom = computeDominators(F);
  std::set<const BasicBlock *> loopHeaders;
  for (auto &BB : F)
    for (const BasicBlock *succ : successors(&BB))
      if (dom[&BB].count(succ))
        loopHeaders.insert(succ);
        
  for (auto &BB : F) {
    unsigned depth = 0;
    for (const BasicBlock *header : loopHeaders)
      if (dom[&BB].count(header))
        depth++;
    depths[&BB] = depth;
  }
  return depths;
}

static double depthToWeight(unsigned depth) {
  return std::pow(10.0, static_cast<double>(depth));
}

//===----------------------------------------------------------------------===//
// EnergyModel
//===----------------------------------------------------------------------===//
struct EnergyModel {
  std::string archName;
  std::string source;
  std::map<std::string, double> costs;

  void loadDefaults(StringRef arch) {
    source = "default";
    std::string archLower = arch.lower();
    if (archLower.find("x86") != std::string::npos || archLower.find("amd64") != std::string::npos) {
      archName = "x86_64";
      costs = {{"Integer",1.0},{"FloatScalar",1.8},{"FloatVector",3.0},
               {"Memory",2.5},{"Branch",2.0},{"Call",3.5},{"Other",0.1}};
    } else {
      archName = "aarch64";
      costs = {{"Integer",1.0},{"FloatScalar",1.5},{"FloatVector",2.5},
               {"Memory",2.0},{"Branch",1.5},{"Call",3.0},{"Other",0.1}};
    }
  }

  double cost(InstrCategory C) const {
    auto it = costs.find(categoryKey(C).str());
    return (it != costs.end()) ? it->second : 0.1;
  }
};

static std::string detectArch(const Module &M) {
  Triple T(M.getTargetTriple());
  switch (T.getArch()) {
    case Triple::aarch64:
    case Triple::aarch64_be: return "aarch64";
    case Triple::x86_64:
    case Triple::x86:        return "x86_64";
    default:                 return "x86_64"; // Safe fallback for standard CI
  }
}

static EnergyModel loadModel(StringRef jsonPath, StringRef arch) {
  EnergyModel model;
  auto bufOrErr = MemoryBuffer::getFile(jsonPath);
  if (!bufOrErr) {
    errs() << "[EnergyPass] Warning: cannot open " << jsonPath
           << " — using built-in defaults\n";
    model.loadDefaults(arch);
    return model;
  }
  auto parsed = json::parse((*bufOrErr)->getBuffer());
  if (!parsed) {
    errs() << "[EnergyPass] Warning: JSON parse error — using built-in defaults\n";
    model.loadDefaults(arch);
    return model;
  }
  auto *root    = parsed->getAsObject();
  auto *archs   = root  ? root->getObject("architectures") : nullptr;
  auto *archObj = archs ? archs->getObject(arch)           : nullptr;
  if (!archObj) {
    errs() << "[EnergyPass] Warning: arch '" << arch
           << "' not in model — using built-in defaults\n";
    model.loadDefaults(arch);
    return model;
  }
  model.archName = arch.str();
  model.source   = "json";
  for (auto &kv : *archObj)
    if (auto v = kv.second.getAsNumber())
      model.costs[kv.first.str()] = *v;
  return model;
}

//===----------------------------------------------------------------------===//
// Result structs
//===----------------------------------------------------------------------===//
struct BlockResult {
  std::string name;
  unsigned    instrCount      = 0;
  double      staticEnergy    = 0.0;
  double      weightedEnergy  = 0.0;
  double      frequencyWeight = 1.0;
  unsigned    loopDepth       = 0;
  std::map<InstrCategory, unsigned> categoryCounts;
  std::string srcFile;
  unsigned    srcLine         = 0;
  unsigned    srcCol          = 0;
  const Instruction *firstDebugInstr = nullptr;
};

struct FunctionResult {
  std::string name;
  std::string arch;
  std::string modelSource;
  unsigned    blockCount     = 0;
  unsigned    instrCount     = 0;
  double      staticEnergy   = 0.0;
  double      weightedEnergy = 0.0;
  std::map<InstrCategory, unsigned> categoryCounts;
  std::map<InstrCategory, double>   weightedCosts;
  std::vector<BlockResult>          blocks;
};

//===----------------------------------------------------------------------===//
// Formatting helpers
//===----------------------------------------------------------------------===//
static std::string fmt(double v, int prec = 4) {
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(prec) << v;
  return ss.str();
}

static std::string pct(double part, double total) {
  if (total == 0.0) return "  0.0%";
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(1) << (100.0 * part / total) << "%";
  return ss.str();
}

static void printSeparator(unsigned width = 62) {
  errs() << "  " << std::string(width, '-') << "\n";
}

//===----------------------------------------------------------------------===//
// Per-block analysis
//===----------------------------------------------------------------------===//
static BlockResult analyseBlock(
    const BasicBlock &BB,
    const EnergyModel &model,
    const std::map<const BasicBlock *, unsigned> &depths) {

  BlockResult result;
  auto dit = depths.find(&BB);
  result.loopDepth       = (dit != depths.end()) ? dit->second : 0;
  result.frequencyWeight = depthToWeight(result.loopDepth);

  if (BB.hasName())
    result.name = BB.getName().str();
  else {
    std::string tmp;
    raw_string_ostream rso(tmp);
    BB.printAsOperand(rso, false);
    result.name = rso.str();
  }

  for (auto &I : BB) {
    if (const DebugLoc &DL = I.getDebugLoc()) {
      if (result.srcLine == 0) {
        result.srcLine         = DL.getLine();
        result.srcCol          = DL.getCol();
        result.firstDebugInstr = &I;
        if (auto *scope = DL->getScope())
          result.srcFile = scope->getFilename().str();
      }
    }
    InstrCategory cat = classify(I);
    result.categoryCounts[cat]++;
    result.staticEnergy += model.cost(cat);
    result.instrCount++;
  }

  result.weightedEnergy = result.staticEnergy * result.frequencyWeight;
  return result;
}

//===----------------------------------------------------------------------===//
// Stderr report
//===----------------------------------------------------------------------===//
static void printBlockResult(const BlockResult &B, const EnergyModel &model) {
  errs() << "\n  [Block: " << B.name << "]";
  if (B.srcLine > 0)
    errs() << "  src=" << B.srcFile << ":" << B.srcLine;
  errs() << "\n"
         << "    loop-depth="     << B.loopDepth
         << "  freq-weight="      << fmt(B.frequencyWeight, 1)
         << "  static-energy="    << fmt(B.staticEnergy)
         << "  weighted-energy="  << fmt(B.weightedEnergy) << "\n";
  if (B.categoryCounts.empty()) return;
  errs() << "    Category       Count  Static    Weighted   %W-Total\n";
  errs() << "    " << std::string(54, '-') << "\n";
  for (auto cat : ALL_CATEGORIES) {
    auto it = B.categoryCounts.find(cat);
    if (it == B.categoryCounts.end() || it->second == 0) continue;
    double sc = it->second * model.cost(cat);
    double wc = sc * B.frequencyWeight;
    errs() << "    " << categoryName(cat)
           << "  " << it->second
           << "      " << fmt(sc)
           << "    "   << fmt(wc)
           << "    "   << pct(wc, B.weightedEnergy) << "\n";
  }
}

static void printFunctionResult(const FunctionResult &F,
                                const EnergyModel &model) {
  errs() << "\n";
  printSeparator();
  errs() << "  [Function: " << F.name << "]\n";
  errs() << "  arch="    << F.arch
         << "  model="   << F.modelSource
         << "  blocks="  << F.blockCount
         << "  instrs="  << F.instrCount << "\n";
  errs() << "  static-energy="   << fmt(F.staticEnergy)
         << "  weighted-energy=" << fmt(F.weightedEnergy) << "\n";
  printSeparator();
  for (auto &B : F.blocks)
    printBlockResult(B, model);
  errs() << "\n  [Function Summary: " << F.name << "]\n";
  errs() << "  Category       Count  Static    Weighted   %W-Total\n";
  printSeparator(54);
  for (auto cat : ALL_CATEGORIES) {
    auto cit = F.categoryCounts.find(cat);
    if (cit == F.categoryCounts.end() || cit->second == 0) continue;
    double sc = cit->second * model.cost(cat);
    auto wit  = F.weightedCosts.find(cat);
    double wc = (wit != F.weightedCosts.end()) ? wit->second : sc;
    errs() << "  " << categoryName(cat)
           << "  " << cit->second
           << "      " << fmt(sc)
           << "    "   << fmt(wc)
           << "    "   << pct(wc, F.weightedEnergy) << "\n";
  }
  printSeparator(54);
  errs() << "  Total instrs          : " << F.instrCount << "\n";
  errs() << "  Total static energy   : " << fmt(F.staticEnergy)   << "\n";
  errs() << "  Total weighted energy : " << fmt(F.weightedEnergy)
         << " (relative units)\n";
  errs() << "  Frequency method      : loop-depth proxy (10^depth)\n";
}

//===----------------------------------------------------------------------===//
// Remark emission via LLVMContext::diagnose()
//===----------------------------------------------------------------------===//
static void emitFunctionRemark(const FunctionResult &F, const Function &Fn) {
  DebugLoc fnLoc;
  if (!Fn.empty()) {
    const BasicBlock *entryBB = &Fn.getEntryBlock();
    for (auto &I : *entryBB) {
      if (I.getDebugLoc()) { fnLoc = I.getDebugLoc(); break; }
    }
    
    // Safety Fallback: Don't pass an uninitialized DebugLoc to diagnose engine
    if (!fnLoc) return;

    std::string msg;
    raw_string_ostream rso(msg);
    rso << "function '" << F.name << "'"
        << " arch=" << F.arch
        << " static-energy=" << fmt(F.staticEnergy)
        << " weighted-energy=" << fmt(F.weightedEnergy)
        << " instrs=" << F.instrCount;

    OptimizationRemarkAnalysis R("energy", "FunctionEnergy", fnLoc, entryBB);
    R << rso.str();
    Fn.getContext().diagnose(R);
  }
}

static void emitBlockRemark(const BlockResult &B, const Function &Fn) {
  if (B.loopDepth == 0 || !B.firstDebugInstr || !B.firstDebugInstr->getDebugLoc()) 
    return;

  std::string msg;
  raw_string_ostream rso(msg);
  rso << "hot block '" << B.name << "'"
      << " loop-depth=" << B.loopDepth
      << " freq-weight=" << fmt(B.frequencyWeight, 1)
      << " static-energy=" << fmt(B.staticEnergy)
      << " weighted-energy=" << fmt(B.weightedEnergy);

  OptimizationRemarkAnalysis R(
      "energy", "HotBlock",
      B.firstDebugInstr->getDebugLoc(),
      B.firstDebugInstr->getParent());
  R << rso.str();
  Fn.getContext().diagnose(R);
}

//===----------------------------------------------------------------------===//
// EnergyPass Registration
//===----------------------------------------------------------------------===//
namespace {

struct ModuleAccumulator {
  unsigned funcCount      = 0;
  unsigned instrCount     = 0;
  double   staticEnergy   = 0.0;
  double   weightedEnergy = 0.0;
};

struct EnergyPass : public PassInfoMixin<EnergyPass> {
  std::string modelPath;
  std::shared_ptr<ModuleAccumulator> modAcc;

  EnergyPass(std::string path, std::shared_ptr<ModuleAccumulator> acc)
      : modelPath(std::move(path)), modAcc(std::move(acc)) {}

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    if (F.isDeclaration() || F.empty())
      return PreservedAnalyses::all();

    auto depths = computeLoopDepths(F);
    const Module *M   = F.getParent();
    std::string arch  = detectArch(*M);
    EnergyModel model = loadModel(modelPath, arch);

    FunctionResult result;
    result.name        = F.getName().str();
    result.arch        = model.archName;
    result.modelSource = model.source;
    result.blockCount  = F.size();

    for (auto &BB : F) {
      BlockResult br = analyseBlock(BB, model, depths);
      result.staticEnergy   += br.staticEnergy;
      result.weightedEnergy += br.weightedEnergy;
      result.instrCount     += br.instrCount;
      for (auto &[cat, cnt] : br.categoryCounts) {
        result.categoryCounts[cat] += cnt;
        result.weightedCosts[cat] += cnt * model.cost(cat) * br.frequencyWeight;
      }
      result.blocks.push_back(std::move(br));
    }

    modAcc->funcCount++;
    modAcc->instrCount     += result.instrCount;
    modAcc->staticEnergy   += result.staticEnergy;
    modAcc->weightedEnergy += result.weightedEnergy;

    printFunctionResult(result, model);

    emitFunctionRemark(result, F);
    for (auto &B : result.blocks)
      emitBlockRemark(B, F);

    return PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};

} // anonymous namespace

llvm::PassPluginLibraryInfo getEnergyPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "EnergyPass", LLVM_VERSION_STRING,
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, FunctionPassManager &FPM,
           ArrayRef<PassBuilder::PipelineElement>) -> bool {
          if (Name != "energy-pass")
            return false;
          std::string modelPath;
          if (const char *env = std::getenv("ENERGY_MODEL_PATH"))
            modelPath = env;
          else
            modelPath = "data/energy_model.json";
          auto acc = std::make_shared<ModuleAccumulator>();
          FPM.addPass(EnergyPass(modelPath, acc));
          return true;
        });
    }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getEnergyPassPluginInfo();
}
