set(CMAKE_CXX_STANDARD 14)
file(GLOB workloadmanager_SOURCES "*.cpp")
file(GLOB workloadmanager_HEADERS "*.h")

add_library(workloadmanager
    ${workloadmanager_SOURCES} ${workloadmanager_HEADERS}
)

target_link_libraries(workloadmanager rmcommon platformcontrol policymanager)
target_include_directories(workloadmanager INTERFACE .)
