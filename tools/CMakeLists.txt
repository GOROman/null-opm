add_library(nullopm_tools_common STATIC
  common/wav.cpp
  common/vgm.cpp
  common/resampler.cpp
  common/player.cpp
)
target_include_directories(nullopm_tools_common PUBLIC common)
target_compile_features(nullopm_tools_common PUBLIC cxx_std_17)
target_link_libraries(nullopm_tools_common PUBLIC nullopm)

find_package(ZLIB QUIET)
if(ZLIB_FOUND)
  target_compile_definitions(nullopm_tools_common PRIVATE NULLOPM_HAVE_ZLIB)
  target_link_libraries(nullopm_tools_common PUBLIC ZLIB::ZLIB)
  message(STATUS "null-opm: zlib found, .vgz supported")
endif()

add_executable(vgm2wav vgm2wav/main.cpp)
target_link_libraries(vgm2wav PRIVATE nullopm_tools_common)

add_executable(wavdiff wavdiff/main.cpp)
target_link_libraries(wavdiff PRIVATE nullopm_tools_common)
