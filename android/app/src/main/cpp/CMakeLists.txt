cmake_minimum_required(VERSION 3.22.1)
project(eejni LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# مفسّر EE# يعيش في core/ في جذر المستودع
set(EE_CORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../core)

add_library(eejni SHARED jni_bridge.cpp ${EE_CORE}/ee.cpp)
target_include_directories(eejni PRIVATE ${EE_CORE})
