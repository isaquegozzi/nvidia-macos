cmake_minimum_required(VERSION 3.20)

project(nvidia-macos
  VERSION 0.0.1
  DESCRIPTION "Fase 0 — Linux GPU Research Lab (GA106 / RTX 3060, somente leitura)"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTING "Build smoke tests" ON)

add_subdirectory(common)

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
  include(CTest)
  add_subdirectory(tests)
endif()
