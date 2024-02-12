cmake_minimum_required(VERSION 3.20)

project(opendds-monitor-tests VERSION 0.0.1 LANGUAGES CXX)

add_library(test_common INTERFACE)
target_compile_options(test_common INTERFACE $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wall -Wpedantic -Wno-unused -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self -Wmissing-include-dirs -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-conversion -Wstrict-overflow=5 -Wundef -Werror> $<$<CXX_COMPILER_ID:MSVC>: /W4>)
if (MSVC)
  target_compile_definitions(test_common INTERFACE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_features(test_common INTERFACE cxx_std_17)
endif()
target_include_directories(test_common INTERFACE
  ../src
)


add_executable(managed_testapp managed.cpp)
OPENDDS_TARGET_SOURCES(managed_testapp test.idl OPENDDS_IDL_OPTIONS "-Gxtypes-complete" SUPPRESS_ANYS OFF)
target_link_libraries(managed_testapp OpenDDW test_common)

add_executable(unmanaged_testapp unmanaged.cpp)
OPENDDS_TARGET_SOURCES(unmanaged_testapp test.idl OPENDDS_IDL_OPTIONS "-Gxtypes-complete" SUPPRESS_ANYS OFF)
target_link_libraries(unmanaged_testapp OpenDDS::Dcps test_common)
