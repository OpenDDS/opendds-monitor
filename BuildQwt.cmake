# Download and build Qwt from source against the Qt version installed in CI.
#
# Run this as a CMake script:
# cmake [-D name=value]... -P BuildQwt.cmake

set(GIT_URL "https://git.code.sf.net/p/qwt/git")
set(GIT_BRANCH "qwt-6.2")

if(NOT DEFINED BUILD_TYPE)
  set(BUILD_TYPE "release")
endif()

if(NOT IS_DIRECTORY "qwt")
  execute_process(COMMAND git clone --depth 1 -b ${GIT_BRANCH} ${GIT_URL} qwt
                  COMMAND_ECHO STDOUT
                  COMMAND_ERROR_IS_FATAL ANY)
endif()

set(_qmake_hints "$ENV{QT_ROOT_DIR}/bin")
if(QT_VERSION_MAJOR)
  set(_qt_dir_env "Qt${QT_VERSION_MAJOR}_DIR")
  list(APPEND _qmake_hints
    "$ENV{${_qt_dir_env}}/bin"
    "$ENV{${_qt_dir_env}}/../.."
    "$ENV{${_qt_dir_env}}/../../bin"
    "$ENV{${_qt_dir_env}}/../../../bin"
  )
endif()

find_program(QMAKE_EXECUTABLE
  NAMES qmake qmake6 qmake-qt6 qmake-qt5
  HINTS ${_qmake_hints}
)

if(NOT QMAKE_EXECUTABLE)
  message(FATAL_ERROR "Could not find qmake for Qt ${QT_VERSION_MAJOR}")
endif()

execute_process(COMMAND "${QMAKE_EXECUTABLE}" qwt.pro
                WORKING_DIRECTORY "qwt"
                COMMAND_ECHO STDOUT
                COMMAND_ERROR_IS_FATAL ANY)

if(WIN32)
  set(MAKE_PROGRAM nmake)
  set(MAKE_TARGET ${BUILD_TYPE})
else()
  find_program(MAKE_PROGRAM NAMES make gmake)
  if(NOT MAKE_PROGRAM)
    message(FATAL_ERROR "Could not find make")
  endif()
  set(MAKE_TARGET)
endif()

execute_process(COMMAND "${MAKE_PROGRAM}" sub-src-qmake_all
                WORKING_DIRECTORY "qwt"
                COMMAND_ECHO STDOUT
                COMMAND_ERROR_IS_FATAL ANY)

execute_process(COMMAND "${MAKE_PROGRAM}" ${MAKE_TARGET}
                WORKING_DIRECTORY "qwt/src"
                COMMAND_ECHO STDOUT
                COMMAND_ERROR_IS_FATAL ANY)
