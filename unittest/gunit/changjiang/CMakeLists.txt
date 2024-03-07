# Copyright (c) 2013, 2022, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/sql
  ${CMAKE_SOURCE_DIR}/storage/changjiang
  ${CMAKE_SOURCE_DIR}/storage/changjiang/common
  ${CMAKE_SOURCE_DIR}/storage/changjiang/core
  ${CMAKE_SOURCE_DIR}/storage/changjiang/fsb
  ${CMAKE_SOURCE_DIR}/storage/changjiang/handler
  ${CMAKE_SOURCE_DIR}/storage/changjiang/memory
  ${CMAKE_SOURCE_DIR}/storage/changjiang/netfra
  ${CMAKE_SOURCE_DIR}/storage/changjiang/streamlog
  ${CMAKE_SOURCE_DIR}/storage/changjiang/thread
  ${CMAKE_SOURCE_DIR}/storage/changjiang/TransformLib
  ${CMAKE_SOURCE_DIR}/storage/changjiang/type
  ${CMAKE_SOURCE_DIR}/storage/changjiang/util  
)

INCLUDE(${CMAKE_SOURCE_DIR}/storage/changjiang/changjiang.cmake)

SET(TESTS
  #example
  storage_handler
)

ADD_COMPILE_FLAGS(
  ut0rnd-t.cc
  COMPILE_FLAGS -I${CMAKE_SOURCE_DIR}/extra/lz4 -I${BUNDLED_LZ4_PATH}
  )

FOREACH(test ${TESTS})
  SET(SRC_FILES ${test}-t.cc)
  MYSQL_ADD_EXECUTABLE(${test}-t ${SRC_FILES}
    ENABLE_EXPORTS
    EXCLUDE_FROM_ALL
    SKIP_INSTALL
    LINK_LIBRARIES gunit_large server_unittest_library changjiang sql_main
    DEPENDENCIES GenError
    )    
ENDFOREACH()
