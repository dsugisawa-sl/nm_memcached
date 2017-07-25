CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
SET(PRJ nm_memcached)
PROJECT(${PRJ})

INCLUDE_DIRECTORIES("deps/netmap/sys/")
INCLUDE_DIRECTORIES(".")


IF (APPLE)
    INCLUDE_DIRECTORIES("/usr/local/mysql/include/")
    INCLUDE_DIRECTORIES("/usr/local/include/")
    INCLUDE_DIRECTORIES("/usr/local/include/mysql-binary-log-events")
    LINK_DIRECTORIES("/usr/local/lib/")
    LINK_DIRECTORIES("/usr/local/mysql/lib/")

else()
    INCLUDE_DIRECTORIES("/usr/local/mysql/include/")
    INCLUDE_DIRECTORIES("/usr/local/include/mysql-binary-log-events")
    LINK_DIRECTORIES("/usr/local/mysql/lib/")
endif()


SET(CMAKE_CXX_STANDARD 11)
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -std=c++11 -O0 -g ")


SET(SOURCE_FILES
        src/entry.cc
        src/nmmc_util.cc
        src/nmmc_process.cc
        src/nmmc_thread.cc
        src/nmmc_server.cc
        src/nmmc_packet.cc
        src/nmmc_arch_if.cc
        src/nmmc_arch_impl_socket.cc
        src/nmmc_arch_impl_netmap.cc
        inc/nmmc_tpl.hpp
        inc/nmmc_tpl.hh )

add_executable(${PRJ} ${SOURCE_FILES})
TARGET_LINK_LIBRARIES(${PRJ} pthread)
TARGET_LINK_LIBRARIES(${PRJ} event)
TARGET_LINK_LIBRARIES(${PRJ} event_pthreads)
TARGET_LINK_LIBRARIES(${PRJ} mysqlclient)
TARGET_LINK_LIBRARIES(${PRJ} binlogevents)
TARGET_LINK_LIBRARIES(${PRJ} mysqlstream)

IF (UNIX AND NOT APPLE)
    TARGET_LINK_LIBRARIES(${PRJ} rt)
ENDIF()


ENABLE_TESTING()
SUBDIRS(test/)
