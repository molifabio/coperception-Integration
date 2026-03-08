set(CMAKE_CXX_STANDARD 14)
file(GLOB testsensors_SOURCES "*.cpp")
file(GLOB testsensors_HEADERS "*.h")

find_package(Sensors REQUIRED)

add_executable(testsensors ${testsensors_HEADERS} ${testsensors_SOURCES})
target_include_directories(testsensors PRIVATE ${SENSORS_INCLUDE_DIR})
target_link_libraries(testsensors ${SENSORS_LIBRARY})
