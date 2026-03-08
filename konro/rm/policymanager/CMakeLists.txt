set(CMAKE_CXX_STANDARD 14)
file(GLOB policymanager_SOURCES "*.cpp")
file(GLOB policymanager_HEADERS "*.h")
file(GLOB policies_SOURCES "policies/*.cpp")
file(GLOB policies_HEADERS "policies/*.h")
add_library(policymanager
            ${policymanager_HEADERS}
            ${policymanager_SOURCES}
            ${policies_HEADERS}
            ${policies_SOURCES}
)

target_link_libraries(policymanager PUBLIC rmcommon platformdescription platformcontrol)
target_include_directories(policymanager INTERFACE .)
