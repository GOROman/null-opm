# Built only under Emscripten: emcmake cmake --preset wasm && cmake --build --preset wasm
if(NOT EMSCRIPTEN)
  return()
endif()
add_executable(nullopm_wasm wasm_main.cpp)
target_link_libraries(nullopm_wasm PRIVATE nullopm)
set_target_properties(nullopm_wasm PROPERTIES OUTPUT_NAME "nullopm" SUFFIX ".js")
target_link_options(nullopm_wasm PRIVATE
  -sMODULARIZE=1 -sEXPORT_NAME=createNullOpm -sENVIRONMENT=web
  -sALLOW_MEMORY_GROWTH=0 -sINITIAL_MEMORY=4194304 -sSTACK_SIZE=65536
  -sEXPORTED_FUNCTIONS=['_malloc','_free','_nullopm_create','_nullopm_destroy','_nullopm_reset','_nullopm_write_reg','_nullopm_read_status','_nullopm_generate_i16','_nullopm_sample_rate','_nullopm_version']
  -sEXPORTED_RUNTIME_METHODS=['HEAP16','UTF8ToString']
  --no-entry)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/index.html ${CMAKE_CURRENT_BINARY_DIR}/index.html COPYONLY)
