set(CMAKE_CXX_STANDARD 14)

find_package(Sensors REQUIRED)

file(GLOB platformmonitor_SOURCES "*.cpp")
file(GLOB platformmonitor_HEADERS "*.h")
add_library(platformmonitor
            ${platformmonitor_HEADERS}
            ${platformmonitor_SOURCES}
)

target_include_directories(platformmonitor INTERFACE .)
target_include_directories(platformmonitor PRIVATE ${SENSORS_INCLUDE_DIR})
target_link_libraries(platformmonitor PUBLIC policymanager  ${SENSORS_LIBRARY})
