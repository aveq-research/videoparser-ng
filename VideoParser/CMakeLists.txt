project(VideoParser)

add_library(VideoParser STATIC VideoParser.cpp VideoParser.h)

# Link VideoParser against FFmpeg statically
target_include_directories(VideoParser PRIVATE ${CMAKE_SOURCE_DIR}/external/ffmpeg)
target_link_libraries(VideoParser PRIVATE ${CMAKE_SOURCE_DIR}/external/ffmpeg/libavcodec/libavcodec.a)
target_link_libraries(VideoParser PRIVATE ${CMAKE_SOURCE_DIR}/external/ffmpeg/libavformat/libavformat.a)
target_link_libraries(VideoParser PRIVATE ${CMAKE_SOURCE_DIR}/external/ffmpeg/libavutil/libavutil.a)
target_link_libraries(VideoParser PRIVATE bz2 z iconv)

# Build FFmpeg if it doesn't exist, or if any of its source files have changed
add_custom_command(
  TARGET VideoParser
  PRE_BUILD
  COMMAND ${CMAKE_SOURCE_DIR}/util/build-ffmpeg.sh
  COMMENT "Checking if FFmpeg needs to be rebuilt"
)
