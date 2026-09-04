add_library(nvidia_common STATIC
  src/common.cpp
)

target_include_directories(nvidia_common
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(nvidia_common PUBLIC cxx_std_20)

# Fase 0: sem warnings-as-errors globais ainda; apenas baseline portátil.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(nvidia_common PRIVATE -Wall -Wextra -Wpedantic)
endif()
