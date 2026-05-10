add_llvm_pass_plugin(EnergyPass
  EnergyPass.cpp
  DEPENDS
    intrinsics_gen
)

target_link_libraries(EnergyPass PRIVATE
  LLVMCore
  LLVMSupport
  LLVMPasses
  LLVMAnalysis
)
