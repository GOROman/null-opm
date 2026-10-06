cmake_minimum_required(VERSION 3.16)
project(nullopm VERSION 0.1.0 LANGUAGES CXX)

if(NOT DEFINED PROJECT_IS_TOP_LEVEL)
  if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(PROJECT_IS_TOP_LEVEL ON)
  else()
    set(PROJECT_IS_TOP_LEVEL OFF)
  endif()
endif()

option(NULLOPM_BUILD_TOOLS "Build command line tools" ${PROJECT_IS_TOP_LEVEL})
option(NULLOPM_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
option(NULLOPM_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})
option(NULLOPM_WARNINGS_AS_ERRORS "Treat core warnings as errors" ${PROJECT_IS_TOP_LEVEL})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(nullopm STATIC
  src/opm.cpp
  src/opm_c.cpp
)
add_library(nullopm::nullopm ALIAS nullopm)
target_include_directories(nullopm PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>)
target_compile_features(nullopm PUBLIC cxx_std_17)

if(MSVC)
  target_compile_options(nullopm PRIVATE /W4 /GR-)
  if(NULLOPM_WARNINGS_AS_ERRORS)
    target_compile_options(nullopm PRIVATE /WX)
  endif()
else()
  target_compile_options(nullopm PRIVATE
    -Wall -Wextra -Wconversion -Wsign-conversion -Wshadow
    -fno-exceptions -fno-rtti)
  if(NULLOPM_WARNINGS_AS_ERRORS)
    target_compile_options(nullopm PRIVATE -Werror)
  endif()
endif()

if(NULLOPM_BUILD_TOOLS OR NULLOPM_BUILD_TESTS)
  add_subdirectory(tools)
endif()
if(NULLOPM_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
if(NULLOPM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

include(GNUInstallDirs)
install(TARGETS nullopm EXPORT nullopmTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY src/nullopm DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
install(EXPORT nullopmTargets NAMESPACE nullopm::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nullopm)
