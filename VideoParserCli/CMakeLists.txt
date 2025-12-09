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
target_include_directories(video-parser PRIVATE ${CMAKE_SOURCE_DIR}/external/ffmpeg)
target_include_directories(video-parser PRIVATE ${CMAKE_SOURCE_DIR}/VideoParserCli/include)

target_link_libraries(video-parser PRIVATE videoparser cxxopts)
