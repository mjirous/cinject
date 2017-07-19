cmake_minimum_required (VERSION 3.0)

SET (PROJECTNAME cinject_test)
project (${PROJECTNAME})

SET (SRCS
	cinject_test.cpp
)

add_executable (${PROJECTNAME} ${SRCS} ${ALL_HEADERS})
target_link_libraries(${PROJECTNAME} ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

add_dependencies(${PROJECT_NAME} _BEFORE_ALL)
add_test(NAME ${PROJECTNAME} COMMAND ${PROJECTNAME})
