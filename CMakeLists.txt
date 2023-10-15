cmake_minimum_required(VERSION 3.15)
project(MyVideoParserProject)

set(CMAKE_CXX_STANDARD 17)

# Include the CMakeLists.txt for the subdirectories
add_subdirectory(VideoParser)
add_subdirectory(VideoParserCli)
