cmake_minimum_required(VERSION 3.16)
project(EEsharp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# عند استعمال core كمشروع فرعي (تطبيق سطح مكتب مُترجَم) نبني المكتبة فقط
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(EE_TOP_LEVEL ON)
else()
  set(EE_TOP_LEVEL OFF)
endif()

find_package(Threads REQUIRED)

# المفسّر + الجلسات + صدفة سطح المكتب
add_library(eelang STATIC ee.cpp ee_host.cpp)
target_include_directories(eelang PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(eelang PUBLIC Threads::Threads)

if(EE_TOP_LEVEL)
  # محرك الترجمة (القوالب تُضمَّن وقت الإعداد)
  include(embed_templates.cmake)
  add_library(eetranslate STATIC translate.cpp)
  target_include_directories(eetranslate PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                             PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

  add_executable(ee main.cpp)
  target_link_libraries(ee PRIVATE eelang eetranslate)

  # واجهة C لتضمين المحرك في WebAssembly أو لغات أخرى
  add_library(eecapi STATIC ee_capi.cpp)
  target_link_libraries(eecapi PUBLIC eelang)

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

  # اختبارات الصدفة والجلسات والمترجم (ملف C++ واحد)
  add_executable(ee_unit ../tests/unit/unit_tests.cpp)
  target_link_libraries(ee_unit PRIVATE eelang eetranslate eecapi)
  add_test(NAME unit COMMAND ee_unit)

  # تطابق ناتج JS المُترجَم مع المفسّر (يتطلب node)
  find_program(EE_NODE node)
  if(EE_NODE)
    foreach(t ${EE_TESTS})
      get_filename_component(n ${t} NAME_WE)
      add_test(NAME js_${n}
               COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/../tests/run_js_test.sh
                       $<TARGET_FILE:ee> ${EE_NODE} ${t}
                       ${CMAKE_CURRENT_SOURCE_DIR}/../tests/${n}.expected)
    endforeach()
    add_test(NAME js_shell
             COMMAND ${EE_NODE} ${CMAKE_CURRENT_SOURCE_DIR}/../tests/shell/shell_test.js $<TARGET_FILE:ee>)
    # متصفح حقيقي (Playwright + Chromium) إن توفّرا
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
      execute_process(COMMAND ${Python3_EXECUTABLE} -c "import playwright" RESULT_VARIABLE EE_PW_RC OUTPUT_QUIET ERROR_QUIET)
      if(EE_PW_RC EQUAL 0)
        add_test(NAME web_e2e
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tests/web/e2e_web.py $<TARGET_FILE:ee>)
      endif()
    endif()
  endif()
endif()
