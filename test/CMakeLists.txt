CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
SET(PRJ nmmc_test)
PROJECT(${PRJ})

INCLUDE_DIRECTORIES("/usr/local/share/include/")
INCLUDE_DIRECTORIES("../../deps/netmap/sys/")
INCLUDE_DIRECTORIES("/usr/local/include/")
INCLUDE_DIRECTORIES("../deps/netmap/sys/")
INCLUDE_DIRECTORIES(".")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2")
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -std=c++11 -O0 -g ")
##
SET(TEST_SOURCE_FILES
        nmmc_test.cc
        )

ADD_EXECUTABLE(${PRJ} ${TEST_SOURCE_FILES})
TARGET_LINK_LIBRARIES(${PRJ} gtest gtest_main)
TARGET_LINK_LIBRARIES(${PRJ} pthread)
TARGET_LINK_LIBRARIES(${PRJ} event)
TARGET_LINK_LIBRARIES(${PRJ} event_pthreads)
ADD_TEST(test ${PRJ})
if (UNIX AND NOT APPLE)
    target_link_libraries(${PRJ} rt)
endif()
