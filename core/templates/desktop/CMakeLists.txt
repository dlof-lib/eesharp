cmake_minimum_required(VERSION 3.16)
project(EEDesktopApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# مسار مجلد core من مشروع EE# (الافتراضي: ../core بجوار هذا المجلد)
set(EE_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../core" CACHE PATH "مجلد core في مشروع EE#")
add_subdirectory(${EE_CORE_DIR} ee-core EXCLUDE_FROM_ALL)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE eelang)
