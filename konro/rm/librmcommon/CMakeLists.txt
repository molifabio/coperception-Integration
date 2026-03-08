set(CMAKE_CXX_STANDARD 14)
file(GLOB librmcommon_SOURCES "*.cpp")
file(GLOB librmcommon_HEADERS "*.h"  "events/*.h")

add_library(rmcommon ${librmcommon_SOURCES} ${librmcommon_HEADERS})

target_include_directories(rmcommon INTERFACE .)
target_include_directories(rmcommon INTERFACE events)
