set(CMAKE_CXX_STANDARD 14)

find_package(PkgConfig REQUIRED)
pkg_check_modules(HWLOC REQUIRED hwloc)

file(GLOB testhwloc_SOURCES "*.cpp")
file(GLOB testhwloc_HEADERS "*.h")

add_executable(testhwloc ${testhwloc_HEADERS} ${testhwloc_SOURCES})
target_link_libraries(testhwloc PRIVATE rmcommon ${HWLOC_LIBRARIES})
target_include_directories(testhwloc PRIVATE ${HWLOC_INCLUDE_DIRS})
