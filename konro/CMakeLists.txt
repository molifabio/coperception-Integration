# To enable timing:
#       cmake -DTIMING=ON

# To override cgroup base directory:
#       cmake -DCGROUPBASEDIR:STRING=/sys/fs/cgroup/unified

cmake_minimum_required (VERSION 3.0.0)
project (konro)

list (APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")

option(TIMING "Enable low level timing" OFF)

if (NOT DEFINED CGROUPBASEDIR)
set(CGROUPBASEDIR "/sys/fs/cgroup" CACHE STRING "The cgroup base directory")
endif()

add_definitions(-DCGROUPBASEDIR="${CGROUPBASEDIR}")

if (TIMING)
    # enable low level timing of (some parts of the) code
    add_definitions(-DTIMING)
endif()

add_subdirectory (common)
add_subdirectory (lib)
add_subdirectory (rm)
add_subdirectory (konrolib)

add_subdirectory (testplatformcontrol)
add_subdirectory (testthreadsafequeue)
add_subdirectory (testsensors)
add_subdirectory (testhwloc)
add_subdirectory (testkonrolib)
add_subdirectory (testtimer)
add_subdirectory (peopledetect)
add_subdirectory (testnamespaces)
add_subdirectory (testrmcommon)
