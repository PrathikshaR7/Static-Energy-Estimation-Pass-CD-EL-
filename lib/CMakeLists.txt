add_llvm_pass_plugin(EnergyPass
  EnergyPass.cpp
  DEPENDS
    intrinsics_gen
)

# Link against LLVM's shared library so all LLVM symbols resolve at load time.
# This is required for out-of-tree plugins on macOS with dylib builds.
target_link_libraries(EnergyPass PRIVATE
  LLVMCore
  LLVMSupport
  LLVMPasses
)
