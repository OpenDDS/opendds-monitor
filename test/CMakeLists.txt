add_executable(managed_testapp
  managed.cpp
)

if (MSVC)
  target_compile_definitions(managed_testapp PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_features(managed_testapp PRIVATE cxx_std_17)
endif()

OPENDDS_TARGET_SOURCES(managed_testapp test.idl OPENDDS_IDL_OPTIONS "-Gxtypes-complete" SUPPRESS_ANYS OFF)

target_link_libraries(managed_testapp
  manager
)

target_include_directories(managed_testapp PRIVATE
  ../src
)

add_executable(unmanaged_testapp
  unmanaged.cpp
)

if (MSVC)
  target_compile_definitions(unmanaged_testapp PRIVATE _CRT_SECURE_NO_WARNINGS)
else ()
  target_compile_features(unmanaged_testapp PRIVATE cxx_std_17)
endif()

OPENDDS_TARGET_SOURCES(unmanaged_testapp test.idl OPENDDS_IDL_OPTIONS "-Gxtypes-complete" SUPPRESS_ANYS OFF)

target_link_libraries(unmanaged_testapp
  ${OPENDDS_TARGETS}
)

target_include_directories(unmanaged_testapp PRIVATE
  ../src
)
