project(VideoParser)

# Add Homebrew paths for Apple Silicon or Intel Mac
if(APPLE)
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
    set(AOM_INCLUDE_DIR "/opt/homebrew/opt/aom/include")
  else()
    list(APPEND CMAKE_PREFIX_PATH "/usr/local")
    set(AOM_INCLUDE_DIR "/usr/local/opt/aom/include")
  endif()
else()
  # On Linux, headers are typically in /usr/include
  set(AOM_INCLUDE_DIR "/usr/include")
endif()

# Find libaom
find_library(AOM_LIBRARY
  NAMES aom
  PATHS
    /opt/homebrew/opt/aom/lib  # Apple Silicon
    /usr/local/opt/aom/lib     # Intel Mac
    /usr/lib                   # Linux
    /usr/lib/x86_64-linux-gnu  # Debian/Ubuntu
  REQUIRED
)

add_library(videoparser STATIC VideoParser.cpp VideoParser.h)

# fix for ffmpeg's use of register keyword
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-register")

# Link VideoParser against FFmpeg statically
target_include_directories(videoparser
  PRIVATE
    ${CMAKE_SOURCE_DIR}/external/ffmpeg
    ${AOM_INCLUDE_DIR}  # Use platform-specific include path
)
target_link_libraries(videoparser PRIVATE ${CMAKE_SOURCE_DIR}/external/ffmpeg/libavcodec/libavcodec.a)
target_link_libraries(videoparser PRIVATE ${CMAKE_SOURCE_DIR}/external/ffmpeg/libavformat/libavformat.a)
target_link_libraries(videoparser PRIVATE ${CMAKE_SOURCE_DIR}/external/ffmpeg/libavutil/libavutil.a)
target_link_libraries(videoparser PRIVATE ${AOM_LIBRARY})
target_link_libraries(videoparser PRIVATE bz2 z)

# Build FFmpeg if it doesn't exist, or if any of its source files have changed
add_custom_command(
  TARGET videoparser
  PRE_BUILD
  COMMAND ${CMAKE_SOURCE_DIR}/util/build-ffmpeg.sh
  COMMENT "Checking if FFmpeg needs to be rebuilt"
)
