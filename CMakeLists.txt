cmake_minimum_required(VERSION 3.20)

project(monitor VERSION 0.0.1 LANGUAGES CXX)

find_package(OpenDDS REQUIRED)

add_subdirectory(thirdparty/OpenDDW)

if(WIN32)
  set(qt_optional_components "")
else()
  set(qt_optional_components DBus)
endif()

find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core Widgets Gui PrintSupport Svg OpenGL OPTIONAL_COMPONENTS ${qt_optional_components})
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Gui PrintSupport Svg OpenGL OPTIONAL_COMPONENTS ${qt_optional_components})

if(QWT_IS_LOCAL)
  set(QWT_LIBRARY ${PROJECT_SOURCE_DIR}/qwt/lib/qwt$<$<CONFIG:DEBUG>:d>.lib)
  set(QWT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/qwt/src)
else()
  # FindQwt.cmake is in this directory
  list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
  find_package(Qwt MODULE REQUIRED)
endif()

set(HEADER
  src/dds_data.h
  src/dynamic_meta_struct.h
  src/editor_delegates.h
  src/first_define.h
  src/graph_page.h
  src/log_page.h
  src/main_window.h
  src/open_dynamic_data.h
  src/participant_page.h
  src/participant_table_model.h
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
  list(APPEND SOURCE ddsmon.rc)
endif(WIN32)

qt_add_resources(RESOURCES ddsmon.qrc)

qt_wrap_cpp(MOC_SOURCE
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

add_executable(monitor
  ${SOURCE}
  ${HEADER}
  ${RESOURCES}
  ${MOC_SOURCE}
  ${UI_SOURCE}
)

set_property(TARGET monitor PROPERTY AUTOUIC TRUE)
set_property(TARGET monitor APPEND PROPERTY AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ui")

target_compile_features(monitor PRIVATE cxx_std_17)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_definitions(monitor PRIVATE _CRT_SECURE_NO_WARNINGS _HAS_AUTO_PTR_ETC=1)

  target_compile_options(monitor PRIVATE
    /W4 # Set the warning level to "Level4"
    /wd4251 #Must have dll-interface to be used by clients of struct.
    /wd4244 #Conversion warning from ACE.
    /wd4250 #inherits via dominance
    /wd4275 #non dll-interface class used as base for dll-interface class
    /WX # Warnings are errors!!!!
  )
endif()

target_compile_options(monitor PRIVATE $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>: -Wall -Wextra -Wpedantic -Wno-unused -Wunused-parameter> $<$<CXX_COMPILER_ID:MSVC>: /W4>)

target_link_libraries(monitor
  OpenDDW
  ${QWT_LIBRARY}
  Qt${QT_VERSION_MAJOR}::PrintSupport
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Gui
  Qt${QT_VERSION_MAJOR}::Svg
  Qt${QT_VERSION_MAJOR}::OpenGL
  $<$<NOT:$<PLATFORM_ID:Windows>>:Qt${QT_VERSION_MAJOR}::DBus>
)

target_include_directories(monitor PRIVATE
  ${QWT_INCLUDE_DIR}
)

configure_file(opendds.ini . COPYONLY)

add_subdirectory(test EXCLUDE_FROM_ALL)
