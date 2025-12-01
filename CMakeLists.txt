cmake_minimum_required(VERSION 3.20)
project(ModernTest VERSION 0.1.0 LANGUAGES CXX)

# 1. The Header-Only Core
add_library(ModernTest_Core INTERFACE)
target_include_directories(ModernTest_Core INTERFACE include)
# Strict C++20 Requirement
target_compile_features(ModernTest_Core INTERFACE cxx_std_20)

# 2. The Runner (compiled main function)
add_library(ModernTest_Runner STATIC src/ModernTestMain.cpp)
target_link_libraries(ModernTest_Runner PUBLIC ModernTest_Core)

# 3. Aliases for Clean Usage
add_library(ModernTest::Core ALIAS ModernTest_Core)
add_library(ModernTest::Runner ALIAS ModernTest_Runner)

# 4. If this is the main project (not fetched), run our own tests
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    enable_testing()
    add_executable(sanity_check tests/sanity_check.cpp)
    target_link_libraries(sanity_check PRIVATE ModernTest::Runner)
    add_test(NAME SanityCheck COMMAND sanity_check)
endif()