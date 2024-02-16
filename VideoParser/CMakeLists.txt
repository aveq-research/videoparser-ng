project(VideoParser)

add_library(videoparser STATIC VideoParser.cpp VideoParser.h)

# fix for ffmpeg's use of register keyword
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-register")

# Link VideoParser against FFmpeg statically
target_include_directories(videoparser PRIVATE ${CMAKE_SOURCE_DIR}/external/ffmpeg)
target_link_libraries(videoparser PRIVATE ${CMAKE_SOURCE_DIR}/external/ffmpeg/libavcodec/libavcodec.a)
target_link_libraries(videoparser PRIVATE ${CMAKE_SOURCE_DIR}/external/ffmpeg/libavformat/libavformat.a)
target_link_libraries(videoparser PRIVATE ${CMAKE_SOURCE_DIR}/external/ffmpeg/libavutil/libavutil.a)
target_link_libraries(videoparser PRIVATE bz2 z)

# Build FFmpeg if it doesn't exist, or if any of its source files have changed
add_custom_command(
  TARGET videoparser
  PRE_BUILD
  COMMAND ${CMAKE_SOURCE_DIR}/util/build-ffmpeg.sh
  COMMENT "Checking if FFmpeg needs to be rebuilt"
)
