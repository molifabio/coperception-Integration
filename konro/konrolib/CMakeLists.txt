set(CMAKE_CXX_STANDARD 14)
file(GLOB konrolib_SOURCES "*.cpp")
file(GLOB konrolib_HEADERS "*.h")

add_library(konrolib ${konrolib_SOURCES} ${konrolib_HEADERS})

target_include_directories(konrolib INTERFACE .)
