# Find Qwt
# ~~~~~~~~
# Copyright (c) 2010, Tim Sutton <tim at linfiniti.com>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# Once run this will define:
#
# QWT_FOUND       = system has QWT lib
# QWT_LIBRARY     = full path to the QWT library
# QWT_INCLUDE_DIR = where to find headers
#


set(QWT_LIBRARY_NAMES qwt-qt5 qwt6-qt5 qwt qwt6)

find_library(QWT_LIBRARY
  NAMES ${QWT_LIBRARY_NAMES}
  PATHS
    /usr/lib
    /usr/local/opt/qwt-qt5/lib
    /usr/local/lib
    /usr/local/lib/qt5
    "$ENV{QWT_DIR}/lib"
    "$ENV{LIB_DIR}/lib"
    "$ENV{LIB}"
)

set(_qwt_fw)
if(QWT_LIBRARY MATCHES "/qwt.*\\.framework")
  string(REGEX REPLACE "^(.*/qwt.*\\.framework).*$" "\\1" _qwt_fw "${QWT_LIBRARY}")
endif()

FIND_PATH(QWT_INCLUDE_DIR NAMES qwt.h PATHS
  "${_qwt_fw}/Headers"
  /usr/include
  /usr/include/qt5
  /usr/local/opt/qwt-qt5/include
  /usr/local/include
  /usr/local/include/qt5
  "$ENV{QWT_DIR}/src"
  "$ENV{LIB_DIR}/include"
  "$ENV{INCLUDE}"
  PATH_SUFFIXES qwt-qt5 qwt qwt6 qt5/qwt
)

if(QWT_INCLUDE_DIR AND QWT_LIBRARY)
  set(Qwt_FOUND TRUE)

  FILE(READ ${QWT_INCLUDE_DIR}/qwt_global.h qwt_header)
  STRING(REGEX REPLACE "^.*QWT_VERSION_STR +\"([^\"]+)\".*$" "\\1" QWT_VERSION_STR "${qwt_header}")

  if(NOT Qwt_FIND_QUIETLY)
    MESSAGE(STATUS "Found Qwt: ${QWT_LIBRARY} (${QWT_VERSION_STR})")
  endif()
else()
  set(Qwt_FOUND FALSE)

  if(Qwt_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find required Qwt")
  elseif(NOT Qwt_FIND_QUIETLY)
    message(STATUS "Could not find optional Qwt")
  endif()
endif()
