project(VideoParser)

# Option to skip ffmpeg build (useful for Docker multi-stage builds where ffmpeg is pre-built)
option(SKIP_FFMPEG_BUILD "Skip automatic ffmpeg build" OFF)

# Vendored libaom paths
set(LIBAOM_SRC_DIR "${CMAKE_SOURCE_DIR}/external/libaom")
set(LIBAOM_BUILD_DIR "${LIBAOM_SRC_DIR}/aom_build")
set(LIBAOM_LIBRARY "${LIBAOM_BUILD_DIR}/libaom.a")

add_library(videoparser STATIC VideoParser.cpp VideoParser.h)

# fix for ffmpeg's use of register keyword
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-register")

# Link VideoParser against FFmpeg and libaom statically
target_include_directories(videoparser
  PRIVATE
    ${CMAKE_SOURCE_DIR}/external/ffmpeg
    ${LIBAOM_SRC_DIR}       # libaom source headers (aom/aom_codec.h etc.)
    ${LIBAOM_BUILD_DIR}     # libaom generated headers (config/aom_config.h etc.)
)
target_link_libraries(videoparser PRIVATE ${CMAKE_SOURCE_DIR}/external/ffmpeg/libavcodec/libavcodec.a)
target_link_libraries(videoparser PRIVATE ${CMAKE_SOURCE_DIR}/external/ffmpeg/libavformat/libavformat.a)
target_link_libraries(videoparser PRIVATE ${CMAKE_SOURCE_DIR}/external/ffmpeg/libavutil/libavutil.a)
target_link_libraries(videoparser PRIVATE ${LIBAOM_LIBRARY})
target_link_libraries(videoparser PRIVATE bz2 z)

# Build FFmpeg (which also builds libaom) if it doesn't exist, or if any of its source files have changed
if(NOT SKIP_FFMPEG_BUILD)
  add_custom_command(
    TARGET videoparser
    PRE_BUILD
    COMMAND ${CMAKE_SOURCE_DIR}/util/build-ffmpeg.sh
    COMMENT "Checking if FFmpeg needs to be rebuilt"
  )
endif()
