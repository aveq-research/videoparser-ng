project(VideoParser)

# Option to skip ffmpeg build (useful for Docker multi-stage builds where ffmpeg is pre-built)
option(SKIP_FFMPEG_BUILD "Skip automatic ffmpeg build" OFF)

# Vendored libaom paths
set(LIBAOM_SRC_DIR "${CMAKE_SOURCE_DIR}/external/libaom")
set(LIBAOM_BUILD_DIR "${LIBAOM_SRC_DIR}/aom_build")
set(LIBAOM_LIBRARY "${LIBAOM_BUILD_DIR}/libaom.a")

# fix for ffmpeg's use of register keyword
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-register")

set(VIDEOPARSER_SOURCES
  VideoParser.cpp VideoParser.h
  videoparser_c.cpp videoparser_c.h
)

if(VIDEOPARSER_SHARED)
  add_library(videoparser SHARED ${VIDEOPARSER_SOURCES})
  target_compile_definitions(videoparser PRIVATE VIDEOPARSER_BUILDING_SHARED)

  # Link VideoParser against the shared FFmpeg, which includes libaom
  target_include_directories(videoparser PRIVATE ${FFMPEG_DIR})
  set(FFMPEG_LIBRARIES
    ${FFMPEG_LIB_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}avformat${CMAKE_SHARED_LIBRARY_SUFFIX}
    ${FFMPEG_LIB_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}avcodec${CMAKE_SHARED_LIBRARY_SUFFIX}
    ${FFMPEG_LIB_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}avutil${CMAKE_SHARED_LIBRARY_SUFFIX}
  )
  target_link_libraries(videoparser PUBLIC ${FFMPEG_LIBRARIES})

  # Find the FFmpeg libraries in the same directory once installed
  if(APPLE)
    set_target_properties(videoparser PROPERTIES INSTALL_RPATH "@loader_path")
  else()
    set_target_properties(videoparser PROPERTIES INSTALL_RPATH "$ORIGIN")
  endif()
else()
  add_library(videoparser STATIC ${VIDEOPARSER_SOURCES})

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
endif()

# Header directory for users of the library in this project (the CLI and the
# C API test)
target_include_directories(videoparser INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

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
if(VIDEOPARSER_SHARED)
  set(VIDEOPARSER_PC_LIBS_PRIVATE "")
else()
  set(VIDEOPARSER_PC_LIBS_PRIVATE "-lavformat -lavcodec -lavutil -laom -latomic -lbz2 -lz -lm -lpthread -lstdc++")
endif()
configure_file(videoparser.pc.in ${CMAKE_CURRENT_BINARY_DIR}/videoparser.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/videoparser.pc DESTINATION lib/pkgconfig)

if(VIDEOPARSER_SHARED)
  install(TARGETS videoparser LIBRARY DESTINATION lib)
  install(FILES VideoParser.h videoparser_c.h DESTINATION include/VideoParser)
  install(DIRECTORY include/ DESTINATION include/VideoParser/include)
  # All shared ffmpeg libraries (with their version symlinks), headers and
  # programs, globbed at install time
  install(CODE "set(FFMPEG_SHARED_PREFIX \"${FFMPEG_SHARED_PREFIX}\")")
  install(CODE [[
    file(GLOB libs "${FFMPEG_SHARED_PREFIX}/lib/lib*.so*" "${FFMPEG_SHARED_PREFIX}/lib/lib*.dylib")
    file(INSTALL ${libs} DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
    file(INSTALL "${FFMPEG_SHARED_PREFIX}/include/" DESTINATION "${CMAKE_INSTALL_PREFIX}/include")
    file(GLOB programs "${FFMPEG_SHARED_PREFIX}/bin/*")
    file(INSTALL ${programs} DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" USE_SOURCE_PERMISSIONS)
  ]])
else()
  install(TARGETS videoparser ARCHIVE DESTINATION lib)
  install(FILES ${FFMPEG_LIBRARIES} ${LIBAOM_LIBRARY} DESTINATION lib)
  install(FILES VideoParser.h videoparser_c.h DESTINATION include/VideoParser)
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
endif()
