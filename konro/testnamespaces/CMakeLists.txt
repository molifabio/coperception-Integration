set(CMAKE_CXX_STANDARD 14)

file(GLOB testnamespaces_SOURCES "*.cpp")
file(GLOB testnamespaces_HEADERS "*.h")

add_executable(testnamespaces ${testnamespaces_HEADERS} ${testnamespaces_SOURCES})
target_link_libraries(testnamespaces rmcommon)
