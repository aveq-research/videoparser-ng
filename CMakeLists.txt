cmake_minimum_required(VERSION 3.15)
project(MyVideoParserProject)

set(CMAKE_CXX_STANDARD 17)

# Installs clang-format pre-commit hook
configure_file("${CMAKE_SOURCE_DIR}/hooks/pre-commit" "${CMAKE_SOURCE_DIR}/.git/hooks/pre-commit" COPYONLY)
execute_process(COMMAND chmod +x "${CMAKE_SOURCE_DIR}/.git/hooks/pre-commit")

# Include the CMakeLists.txt for the subdirectories
add_subdirectory(VideoParser)
add_subdirectory(VideoParserCli)
