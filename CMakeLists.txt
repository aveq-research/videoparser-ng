cmake_minimum_required(VERSION 3.15)
project(MyVideoParserProject)

set(CMAKE_CXX_STANDARD 17)

# Installs clang-format pre-commit hook (only if present, and ${CMAKE_SOURCE_DIR}/.git exists)
if(EXISTS "${CMAKE_SOURCE_DIR}/hooks/pre-commit" AND IS_DIRECTORY "${CMAKE_SOURCE_DIR}/.git")
    configure_file("${CMAKE_SOURCE_DIR}/hooks/pre-commit" "${CMAKE_SOURCE_DIR}/.git/hooks/pre-commit" COPYONLY)
    execute_process(COMMAND chmod +x "${CMAKE_SOURCE_DIR}/.git/hooks/pre-commit")
endif()

# Include the CMakeLists.txt for the subdirectories
add_subdirectory(VideoParser)
add_subdirectory(VideoParserCli)
