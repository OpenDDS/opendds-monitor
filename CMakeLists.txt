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

set(UI
  graph_page.ui
  graph_properties.ui
  log_page.ui
  main_window.ui
  participant_page.ui
  recorder_dialog.ui
  table_page.ui
)

set(HEADER
  dds_callback.h
  dds_data.h
  dds_listeners.h
  dds_logging.h
  dds_manager.h
  dds_manager.hpp
  dds_simple.h
  dynamic_meta_struct.h
  editor_delegates.h
  filesystem.hpp
  first_define.h
  graph_page.h
  log_page.h
  main_window.h
  open_dynamic_data.h
  participant_monitor.h
  participant_page.h
  participant_table_model.h
  platformIndependent.h
  publication_monitor.h
  qos_dictionary.h
  recorder_dialog.h
  subscription_monitor.h
  table_page.h
  topic_monitor.h
  topic_replayer.h
  topic_table_model.h
)

set(SOURCE
  dds_callback.cpp
  dds_data.cpp
  dds_listeners.cpp
  dds_logging.cpp
  dds_manager.cpp
  dynamic_meta_struct.cpp
  editor_delegates.cpp
  graph_page.cpp
  log_page.cpp
  main.cpp
  main_window.cpp
  open_dynamic_data.cpp
  participant_monitor.cpp
  participant_page.cpp
  participant_table_model.cpp
  publication_monitor.cpp
  qos_dictionary.cpp
  recorder_dialog.cpp
  subscription_monitor.cpp
  table_page.cpp
  topic_monitor.cpp
  topic_replayer.cpp
  topic_table_model.cpp
)

# Add the windows explorer icon
if(WIN32)
  set(SOURCE ${SOURCE} ddsmon.rc)
endif(WIN32)

qt5_add_resources(RESOURCES ddsmon.qrc)

qt5_wrap_cpp(MOC_SOURCE
  graph_page.h
  log_page.h
  main_window.h
  participant_page.h
  participant_table_model.h
  publication_monitor.h
  recorder_dialog.h
  subscription_monitor.h
  table_page.h
  topic_table_model.h
)

qt5_wrap_ui(UI_SOURCE ${UI})

add_executable(monitor
  ${HEADER}
  ${SOURCE}
  ${RESOURCES}
  ${MOC_SOURCE}
  ${UI_SOURCE}
)

target_compile_features(monitor PRIVATE cxx_std_17)

if (MSVC)
  target_compile_definitions(monitor PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

target_link_libraries(monitor
  OpenDDS::Dcps
  OpenDDS::Rtps
  OpenDDS::Rtps_Udp
  OpenDDS::Tcp
  ${QWT_LIBRARY}
  Qt5::PrintSupport
  Qt5::Widgets
  Qt5::Core
  Qt5::Gui
  Qt5::Svg
  Qt5::OpenGL
  Threads::Threads
  $<$<NOT:$<PLATFORM_ID:Windows>>:Qt5::DBus>
)

target_include_directories(monitor PRIVATE
  ${QWT_INCLUDE_DIR}
  ${CMAKE_BINARY_DIR}
)

foreach(file opendds.ini)
  configure_file(${CMAKE_SOURCE_DIR}/${file} ${CMAKE_BINARY_DIR}/${file} COPYONLY)
endforeach()

OPENDDS_TARGET_SOURCES(monitor std_qos.idl)
