cmake_minimum_required  (VERSION 2.6)

project (FFMPEG_TEST)

add_executable (ffmpeg_test Main.cxx)

set(CMAKE_SYSTEM_NAME Linux)

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

target_link_libraries(libavformat libavcodec libavutil libswscale pthread)

# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
  "${PROJECT_INCLUDE_DIR}/ffmpeg_test.h.in"
  "${PROJECT_BINARY_DIR}/ffmpeg_test.h"
  )

