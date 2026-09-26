# C API test program: writes the same NDJSON as the CLI, through the C API
# only (see docs/c-api.md)
add_executable(videoparser-c-test videoparser-c-test.c json_double.h)
set_target_properties(videoparser-c-test PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF
)
target_compile_options(videoparser-c-test PRIVATE -Wall -Wextra -pedantic)
target_link_libraries(videoparser-c-test PRIVATE videoparser m)
