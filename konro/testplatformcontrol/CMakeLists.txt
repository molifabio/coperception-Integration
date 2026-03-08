set(CMAKE_CXX_STANDARD 14)

# Test init
include(CTest)
enable_testing()

# Test macro add_unit_test
macro(add_unit_test testname)
  #if(CMAKE_BUILD_TYPE MATCHES "Debug")
    FILE(GLOB sources ${testname}*.cpp unittest.h)
    add_executable(${testname} ${sources})
    add_test(NAME ${testname} COMMAND "${PROJECT_BINARY_DIR}/testplatformcontrol/${testname}")
    target_link_libraries(${testname} PUBLIC rmcommon platformcontrol)
  #endif()
endmacro(add_unit_test)

add_unit_test(test_split)
add_unit_test(test_findcgrouppath)
add_unit_test(test_createcgroup)
add_unit_test(test_keyvalueparser)
add_unit_test(test_cpusetcontrol)



