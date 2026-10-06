add_executable(nullopm_tests
  test_main.cpp
  test_regs.cpp
  test_tables.cpp
  test_pg.cpp
  test_eg.cpp
)
target_link_libraries(nullopm_tests PRIVATE nullopm)
target_include_directories(nullopm_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(nullopm_tests PRIVATE
  NULLOPM_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME nullopm_tests COMMAND nullopm_tests)
