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
    ${FFMPEG_DIR}
    ${LIBAOM_SRC_DIR}       # libaom source headers (aom/aom_codec.h etc.)
    ${LIBAOM_BUILD_DIR}     # libaom generated headers (config/aom_config.h etc.)
)
set(FFMPEG_LIBRARIES
  ${FFMPEG_DIR}/libavcodec/libavcodec.a
  ${FFMPEG_DIR}/libavformat/libavformat.a
  ${FFMPEG_DIR}/libavutil/libavutil.a
)
target_link_libraries(videoparser PUBLIC ${FFMPEG_LIBRARIES})
target_link_libraries(videoparser PUBLIC ${LIBAOM_LIBRARY})
target_link_libraries(videoparser PUBLIC bz2 z)

# Build FFmpeg (which also builds libaom) if it doesn't exist, or if any of its source files have changed
if(NOT SKIP_FFMPEG_BUILD)
  add_custom_command(
    TARGET videoparser
    PRE_BUILD
    COMMAND ${CMAKE_SOURCE_DIR}/util/build-ffmpeg.sh ${FFMPEG_BUILD_ARGS}
    COMMENT "Checking if FFmpeg needs to be rebuilt"
  )
endif()

# SDK layout (lib/ and include/), as in the release SDK archives:
# cmake --install <build dir> --prefix <sdk dir>
install(TARGETS videoparser ARCHIVE DESTINATION lib)
install(FILES ${FFMPEG_LIBRARIES} ${LIBAOM_LIBRARY} DESTINATION lib)
install(FILES VideoParser.h DESTINATION include/VideoParser)
install(DIRECTORY include/ DESTINATION include/VideoParser/include)
install(DIRECTORY ${LIBAOM_SRC_DIR}/aom/ DESTINATION include/aom
  FILES_MATCHING PATTERN "*.h" PATTERN "src" EXCLUDE)
install(FILES ${LIBAOM_BUILD_DIR}/config/aom_config.h DESTINATION include/aom)
# ffmpeg headers are globbed at install time, since the ffmpeg build generates
# some of them
install(CODE "set(FFMPEG_DIR \"${FFMPEG_DIR}\")")
install(CODE [[
  foreach(lib avcodec avformat avutil)
    file(GLOB headers "${FFMPEG_DIR}/lib${lib}/*.h")
    file(INSTALL ${headers} DESTINATION "${CMAKE_INSTALL_PREFIX}/include/lib${lib}")
  endforeach()
]])
