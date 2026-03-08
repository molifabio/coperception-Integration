set(CMAKE_CXX_STANDARD 14)

find_package(PkgConfig REQUIRED)
pkg_check_modules(HWLOC REQUIRED hwloc)

file(GLOB platformdescription_SOURCES "*.cpp")
file(GLOB platformdescription_HEADERS "*.h")

add_library(platformdescription
    ${platformdescription_SOURCES} ${platformdescription_HEADERS}
    )

target_link_libraries(platformdescription PRIVATE rmcommon ${HWLOC_LIBRARIES})
target_include_directories(platformdescription PRIVATE ${HWLOC_INCLUDE_DIRS})
target_include_directories(platformdescription INTERFACE .)

