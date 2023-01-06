cmake_minimum_required(VERSION 3.0)

#cmake_policy(SET CMP0048 NEW)

project(opendds-monitor VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_MODULE_PATH
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_MODULE_PATH}
)
set(CMAKE_PREFIX_PATH
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_PREFIX_PATH}
)

find_package(OpenDDS REQUIRED)

if(WIN32)
  set(qt_optional_components "")
else()
  set(qt_optional_components DBus)
endif()

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui PrintSupport Svg OpenGL OPTIONAL_COMPONENTS ${qt_optional_components})

if (NOT ${Qt5Core_FOUND})
  message(STATUS "Skipping ${PROJECT_NAME} project: Qt5 not found")
  return()
endif (NOT ${Qt5Core_FOUND})

find_package(Qwt MODULE REQUIRED)

if (NOT ${Qwt_FOUND})
  message(STATUS "Skipping ${PROJECT_NAME} project: Qwt not found")
  return()
endif (NOT ${Qwt_FOUND})

set(CMAKE_AUTOMOC FALSE)

set(MANAGER_HEADER
  src/dds_callback.h
  src/dds_listeners.h
  src/dds_logging.h
  src/dds_manager.h
  src/participant_monitor.h
  src/qos_dictionary.h
)

set(MANAGER_SOURCE
  src/dds_callback.cpp
  src/dds_listeners.cpp
  src/dds_logging.cpp
  src/dds_manager.cpp
  src/participant_monitor.cpp
  src/qos_dictionary.cpp
)

set(HEADER
  src/dds_data.h
  src/dynamic_meta_struct.h
  src/editor_delegates.h
  src/filesystem.hpp
  src/first_define.h
  src/graph_page.h
  src/log_page.h
  src/main_window.h
  src/open_dynamic_data.h
  src/participant_page.h
  src/participant_table_model.h
  src/platformIndependent.h
  src/publication_monitor.h
  src/recorder_dialog.h
  src/subscription_monitor.h
  src/table_page.h
  src/topic_monitor.h
  src/topic_replayer.h
  src/topic_table_model.h
)

set(SOURCE
  src/dds_data.cpp
  src/dynamic_meta_struct.cpp
  src/editor_delegates.cpp
  src/graph_page.cpp
  src/log_page.cpp
  src/main.cpp
  src/main_window.cpp
  src/open_dynamic_data.cpp
  src/participant_page.cpp
  src/participant_table_model.cpp
  src/publication_monitor.cpp
  src/recorder_dialog.cpp
  src/subscription_monitor.cpp
  src/table_page.cpp
  src/topic_monitor.cpp
  src/topic_replayer.cpp
  src/topic_table_model.cpp
)

set(UI
  ui/graph_page.ui
  ui/graph_properties.ui
  ui/log_page.ui
  ui/main_window.ui
  ui/participant_page.ui
  ui/recorder_dialog.ui
  ui/table_page.ui
)

# Add the windows explorer icon
if(WIN32)
  set(SOURCE ${SOURCE} ddsmon.rc)
endif(WIN32)

qt5_add_resources(RESOURCES ddsmon.qrc)

qt5_wrap_cpp(MOC_SOURCE
  src/graph_page.h
  src/log_page.h
  src/main_window.h
  src/participant_page.h
  src/participant_table_model.h
  src/publication_monitor.h
  src/recorder_dialog.h
  src/subscription_monitor.h
  src/table_page.h
  src/topic_table_model.h
)

qt5_wrap_ui(UI_SOURCE ${UI})

add_library(manager
  ${MANAGER_HEADER}
  ${MANAGER_SOURCE}
)

if (MSVC)
  target_compile_definitions(manager PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_features(manager PRIVATE cxx_std_17)
endif()

target_link_libraries(manager
  OpenDDS::Dcps
  OpenDDS::Rtps
  OpenDDS::Rtps_Udp
  OpenDDS::Tcp
  OpenDDS::Shmem
  OpenDDS::Udp
  OpenDDS::Multicast
  Threads::Threads
)

target_include_directories(manager PRIVATE
  ${CMAKE_BINARY_DIR}
)

OPENDDS_TARGET_SOURCES(manager idl/std_qos.idl)

add_executable(monitor
  ${MANAGER_HEADER}
  ${HEADER}
  ${SOURCE}
  ${RESOURCES}
  ${MOC_SOURCE}
  ${UI_SOURCE}
)

if (MSVC)
  target_compile_definitions(monitor PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_features(monitor PRIVATE cxx_std_17)
endif()

target_link_libraries(monitor
  manager
  ${QWT_LIBRARY}
  Qt5::PrintSupport
  Qt5::Widgets
  Qt5::Core
  Qt5::Gui
  Qt5::Svg
  Qt5::OpenGL
  $<$<NOT:$<PLATFORM_ID:Windows>>:Qt5::DBus>
)

target_include_directories(monitor PRIVATE
  ${QWT_INCLUDE_DIR}
  ${CMAKE_BINARY_DIR}
)

add_executable(managed_testapp
  ${MANAGER_HEADER}
  test/managed.cpp
)

if (MSVC)
  target_compile_definitions(managed_testapp PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_features(managed_testapp PRIVATE cxx_std_17)
endif()

OPENDDS_TARGET_SOURCES(managed_testapp test/test.idl OPENDDS_IDL_OPTIONS "-Gxtypes-complete" SUPPRESS_ANYS OFF)

target_link_libraries(managed_testapp
  manager
)

target_include_directories(managed_testapp PRIVATE
  ${CMAKE_BINARY_DIR}
  src
)

add_executable(unmanaged_testapp
  test/unmanaged.cpp
)


if (MSVC)
  target_compile_definitions(unmanaged_testapp PRIVATE _CRT_SECURE_NO_WARNINGS)
else ()
  target_compile_features(unmanaged_testapp PRIVATE cxx_std_17)
endif()

OPENDDS_TARGET_SOURCES(unmanaged_testapp test/test.idl OPENDDS_IDL_OPTIONS "-Gxtypes-complete" SUPPRESS_ANYS OFF)

target_link_libraries(unmanaged_testapp
  OpenDDS::Dcps
  OpenDDS::Rtps
  OpenDDS::Rtps_Udp
  OpenDDS::Tcp
  OpenDDS::Shmem
  OpenDDS::Udp
  OpenDDS::Multicast
)

target_include_directories(unmanaged_testapp PRIVATE
  ${CMAKE_BINARY_DIR}
  src
)
