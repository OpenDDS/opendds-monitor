# Download and build the Qwt library from source

# Run this as a CMake script:
# cmake [-D name=value]... -P BuildQwt.cmake

# Requires these programs on the PATH: git
# Assumes the Visual C++ build environment (cl, nmake, etc.) from vcvars64

# Requires these CMake variables (use -D):
# VCPKG_INSTALLED_DIR: the "installed" directory of vcpkg containing qt5
# VCPKG_DEFAULT_TRIPLET: vcpkg's name for the platform, like x64-windows
# BUILD_TYPE: "debug" or "release"

set(GIT_URL "https://git.code.sf.net/p/qwt/git")
set(GIT_BRANCH "qwt-6.2")
set(VCPKG_INST "${VCPKG_INSTALLED_DIR}/${VCPKG_DEFAULT_TRIPLET}")

if(NOT IS_DIRECTORY "qwt")
  execute_process(COMMAND git clone --depth 1 -b ${GIT_BRANCH} ${GIT_URL} qwt
                  COMMAND_ECHO STDOUT
                  COMMAND_ERROR_IS_FATAL ANY)
endif()

# vcpkg's conf files use this variable, which must use forward slashes
file(TO_CMAKE_PATH "${VCPKG_INST}" CURRENT_INSTALLED_DIR)
configure_file("${VCPKG_INST}/tools/qt5/qt_${BUILD_TYPE}.conf" "qwt/qt.conf")

execute_process(COMMAND ${VCPKG_INST}/tools/qt5/bin/qmake -qtconf qt.conf qwt.pro
                WORKING_DIRECTORY "qwt"
                COMMAND_ECHO STDOUT
                COMMAND_ERROR_IS_FATAL ANY)

execute_process(COMMAND nmake sub-src-qmake_all
                WORKING_DIRECTORY "qwt"
                COMMAND_ECHO STDOUT
                COMMAND_ERROR_IS_FATAL ANY)

execute_process(COMMAND nmake ${BUILD_TYPE}
                WORKING_DIRECTORY "qwt/src"
                COMMAND_ECHO STDOUT
                COMMAND_ERROR_IS_FATAL ANY)
