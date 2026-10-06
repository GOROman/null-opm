add_library(nullopm_tools_common STATIC
  common/wav.cpp
)
target_include_directories(nullopm_tools_common PUBLIC common)
target_compile_features(nullopm_tools_common PUBLIC cxx_std_17)
