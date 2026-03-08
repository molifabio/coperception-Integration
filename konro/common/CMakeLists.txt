set (CMAKE_CXX_STANDARD 17)

add_library(konro-common STATIC src/config.cpp)
target_compile_definitions(konro-common PRIVATE KONRO_COMMON)
target_include_directories(konro-common PUBLIC "${PROJECT_SOURCE_DIR}/common/include/")
target_link_libraries(konro-common PRIVATE mini)
