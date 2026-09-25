project(video-parser)

include(FetchContent)
FetchContent_Declare(
    cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    GIT_TAG        v3.2.1
)
FetchContent_MakeAvailable(cxxopts)

add_executable(video-parser main.cpp)

target_include_directories(video-parser PRIVATE ${CMAKE_SOURCE_DIR}/VideoParser)
target_include_directories(video-parser PRIVATE ${FFMPEG_DIR})
target_include_directories(video-parser PRIVATE ${CMAKE_SOURCE_DIR}/VideoParserCli/include)

target_link_libraries(video-parser PRIVATE videoparser cxxopts)

if(VIDEOPARSER_SHARED)
  # Find libvideoparser and the FFmpeg libraries once installed
  if(APPLE)
    set(VIDEOPARSER_CLI_RPATH_DEFAULT "@loader_path/../lib")
  else()
    set(VIDEOPARSER_CLI_RPATH_DEFAULT "$ORIGIN/../lib")
  endif()
  set(VIDEOPARSER_CLI_RPATH "${VIDEOPARSER_CLI_RPATH_DEFAULT}" CACHE STRING "Runpath of the installed CLI")
  set_target_properties(video-parser PROPERTIES INSTALL_RPATH "${VIDEOPARSER_CLI_RPATH}")
  install(TARGETS video-parser RUNTIME DESTINATION bin)
endif()
