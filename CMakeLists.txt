add_llvm_pass_plugin(EnergyPass
  EnergyPass.cpp
)

target_link_libraries(EnergyPass PRIVATE
  LLVMCore
  LLVMSupport
  LLVMPasses
  LLVMAnalysis
)
