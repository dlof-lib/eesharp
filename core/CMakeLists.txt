cmake_minimum_required(VERSION 3.16)
project(EEsharp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(eelang STATIC ee.cpp)
target_include_directories(eelang PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(ee main.cpp)
target_link_libraries(ee PRIVATE eelang)

# ── الاختبارات: لكل ملف tests/*.ee ملف مقابل *.expected (و *.in اختياري للمدخلات)
enable_testing()
file(GLOB EE_TESTS ${CMAKE_CURRENT_SOURCE_DIR}/../tests/*.ee)
foreach(t ${EE_TESTS})
  get_filename_component(n ${t} NAME_WE)
  add_test(NAME ${n}
           COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/../tests/run_test.sh
                   $<TARGET_FILE:ee> ${t}
                   ${CMAKE_CURRENT_SOURCE_DIR}/../tests/${n}.expected)
endforeach()
