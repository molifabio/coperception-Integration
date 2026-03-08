set(CMAKE_CXX_STANDARD 14)

file(GLOB testkonrolib_SOURCES "*.cpp")
file(GLOB testkonrolib_HEADERS "*.h")

add_executable(testkonrolib ${testkonrolib_HEADERS} ${testkonrolib_SOURCES})
target_link_libraries(testkonrolib konrolib)
