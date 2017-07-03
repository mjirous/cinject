cmake_minimum_required (VERSION 3.0)

SET (PROJECTNAME cinject_test)
project (${PROJECTNAME})

SET (SRCS
	cinject_test.cpp
)

add_executable (${PROJECTNAME} ${SRCS})
target_link_libraries(${PROJECTNAME} ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

add_test(NAME ${PROJECTNAME} COMMAND ${PROJECTNAME})
