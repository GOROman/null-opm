# add_subdirectory(path/to/null-opm/platform/pico) then target_link_libraries(app nullopm)
add_library(nullopm INTERFACE)
target_sources(nullopm INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/../../src/opm.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../../src/opm_c.cpp)
target_include_directories(nullopm INTERFACE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_compile_features(nullopm INTERFACE cxx_std_17)
