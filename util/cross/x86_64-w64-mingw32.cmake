# CMake toolchain file for Windows x86_64 with a mingw-w64 cross compiler
# (llvm-mingw or GCC), for example:
#
#   cmake -DCMAKE_TOOLCHAIN_FILE=util/cross/x86_64-w64-mingw32.cmake ...
#
# The compiler is found as x86_64-w64-mingw32-gcc and -g++ in PATH (llvm-mingw
# provides these names for Clang).

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(VP_CROSS_PREFIX x86_64-w64-mingw32-)
set(CMAKE_C_COMPILER ${VP_CROSS_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${VP_CROSS_PREFIX}g++)
set(CMAKE_RC_COMPILER ${VP_CROSS_PREFIX}windres)
set(CMAKE_AR ${VP_CROSS_PREFIX}ar CACHE FILEPATH "Archiver")
set(CMAKE_RANLIB ${VP_CROSS_PREFIX}ranlib CACHE FILEPATH "Indexer")

# Libraries and headers only from the target's sysroot, programs from the host
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
