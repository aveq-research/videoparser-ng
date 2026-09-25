cmake_minimum_required(VERSION 3.15)
project(MyVideoParserProject)

set(CMAKE_CXX_STANDARD 17)

# Installs clang-format pre-commit hook (only if present, and ${CMAKE_SOURCE_DIR}/.git exists)
if(EXISTS "${CMAKE_SOURCE_DIR}/hooks/pre-commit" AND IS_DIRECTORY "${CMAKE_SOURCE_DIR}/.git")
    configure_file("${CMAKE_SOURCE_DIR}/hooks/pre-commit" "${CMAKE_SOURCE_DIR}/.git/hooks/pre-commit" COPYONLY)
    execute_process(COMMAND chmod +x "${CMAKE_SOURCE_DIR}/.git/hooks/pre-commit")
endif()

# Legacy mode: build against the ffmpeg built with VP_MV_POC_NORMALIZATION=1
# by `util/build-ffmpeg.sh --legacy` instead of external/ffmpeg
option(VIDEOPARSER_LEGACY "Build against the legacy-mode ffmpeg" OFF)

if(VIDEOPARSER_LEGACY)
    set(FFMPEG_DIR "${CMAKE_SOURCE_DIR}/build/ffmpeg-legacy/src")
    set(FFMPEG_BUILD_ARGS --legacy)
else()
    set(FFMPEG_DIR "${CMAKE_SOURCE_DIR}/external/ffmpeg")
    set(FFMPEG_BUILD_ARGS "")
endif()

# Include the CMakeLists.txt for the subdirectories
add_subdirectory(VideoParser)
add_subdirectory(VideoParserCli)
