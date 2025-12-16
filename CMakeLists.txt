cmake_minimum_required(VERSION 3.20)
project(ghmwm LANGUAGES C CXX)

# Usage (from repository root or this directory):
#   cmake -S applications/ghmwm -B build-ghmwm \
#         -DCMAKE_TOOLCHAIN_FILE=<path-to-ghost-toolchain.cmake> \
#         -DSYSROOT=<path-to-sysroot>
#   cmake --build build-ghmwm
# SYSROOT must contain the Ghost headers/libs (system/include, system/lib).

if(NOT DEFINED SYSROOT)
  message(FATAL_ERROR "SYSROOT must be set to the Ghost sysroot path")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sources
file(GLOB SRC CONFIGURE_DEPENDS src/*.cpp)
add_executable(ghmwm ${SRC})

# Includes
include_directories(${SYSROOT}/system/include ${CMAKE_SOURCE_DIR}/src)

# Linker search path and rpath
link_directories(${SYSROOT}/system/lib)
target_link_options(ghmwm PRIVATE -L${SYSROOT}/system/lib -Wl,-rpath-link,${SYSROOT}/system/lib)

# Libraries required by ghmwm
set(GHMWM_LIBS
  video
  ps2driver
  input
  properties
  png
  z
)

target_link_libraries(ghmwm PRIVATE ${GHMWM_LIBS})

# Output name (no extension, matches existing app layout)
set_target_properties(ghmwm PROPERTIES OUTPUT_NAME "ghmwm")

# Optional install to sysroot/applications if desired
install(TARGETS ghmwm RUNTIME DESTINATION ${SYSROOT}/applications)
