cmake_minimum_required(VERSION 3.20)

project(minisimt VERSION 0.4.0 LANGUAGES CXX)

add_library(minisimt
    src/model.cpp
    src/trace.cpp
)

target_include_directories(minisimt
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(minisimt PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(minisimt PRIVATE /W4 /permissive-)
else()
    target_compile_options(minisimt PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(minisimt_demo app/main.cpp)
target_link_libraries(minisimt_demo PRIVATE minisimt)

include(CTest)

if(BUILD_TESTING)
    add_executable(minisimt_tests tests/model_tests.cpp)
    target_link_libraries(minisimt_tests PRIVATE minisimt)
    add_test(NAME minisimt_tests COMMAND minisimt_tests)
endif()
