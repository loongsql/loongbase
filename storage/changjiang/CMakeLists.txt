INCLUDE(changjiang.cmake)

ADD_DEFINITIONS(-D_REENTRANT
                -DCHANGJIANGDB -DSTORAGE_ENGINE -DMEM_DEBUG
                -DMYSQL_SERVER)
#               -DNAMESPACE=Nfs

if (UNIX)
ADD_DEFINITIONS(-D_PTHREADS)
endif (UNIX)

#ADD_DEFINITIONS(-D_MSC_VER=1001 -D_PTHREADS)
#ADD_DEFINITIONS(-D_PTHREADS)

if (UNIX)
INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/sql
  ${CMAKE_SOURCE_DIR}/extra/zlib
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
else()
INCLUDE_DIRECTORIES(
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
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/sql
  ${CMAKE_SOURCE_DIR}/extra/zlib
)
endif (UNIX)

file(GLOB COMMON_SRC "common/*.h" "common/*.cc" "common/*.cpp")
file(GLOB CORE_SRC "core/*.h" "core/*.cpp")
file(GLOB FSB_SRC "fsb/*.h" "fsb/*.cpp")
file(GLOB HANDLER_SRC "handler/*.h" "handler/*.cpp")
file(GLOB MEMORY_SRC "memory/*.h" "memory/*.cpp")
file(GLOB NETFRA_SRC "netfra/*.h" "netfra/*.cpp")
file(GLOB STREAMLOG_SRC "streamlog/*.h" "streamlog/*.cpp")
file(GLOB THREAD_SRC "thread/*.h" "thread/*.cpp")
file(GLOB TRANSFORMLIB_SRCS "TransformLib/*.h" "TransformLib/*.cpp")
file(GLOB TYPE_SRC "type/*.h" "type/*.cpp")
file(GLOB UTIL_SRC "util/*.h" "util/*.cpp")

SET(CHANGJIANG_SOURCES
  ${COMMON_SRC} ${CORE_SRC} ${FSB_SRC} ${HANDLER_SRC}
  ${MEMORY_SRC} ${NETFRA_SRC} ${STREAMLOG_SRC} ${THREAD_SRC}
  ${TRANSFORMLIB_SRCS} ${TYPE_SRC} ${UTIL_SRC}
)

MYSQL_ADD_PLUGIN(changjiang ${CHANGJIANG_SOURCES} STORAGE_ENGINE DEFAULT)

SET(CJD_DUMP_LIB changjiang mysqlclient)
MYSQL_ADD_EXECUTABLE(cjddump tools/cjddump.cc)
TARGET_LINK_LIBRARIES(cjddump ${CJD_DUMP_LIB})

