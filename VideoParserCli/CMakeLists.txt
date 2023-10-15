project(video-parser)

add_executable(video-parser main.cpp)

# Link video-parser against VideoParser statically
target_include_directories(video-parser PRIVATE ${CMAKE_SOURCE_DIR}/VideoParser)
target_include_directories(video-parser PRIVATE ${CMAKE_SOURCE_DIR}/external/ffmpeg)
target_include_directories(video-parser PRIVATE ${CMAKE_SOURCE_DIR}/VideoParserCli/include)
target_link_libraries(video-parser PRIVATE VideoParser)
