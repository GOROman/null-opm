idf_component_register(
  SRCS "../../src/opm.cpp" "../../src/opm_c.cpp"
  INCLUDE_DIRS "../../src")
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-exceptions -fno-rtti)
