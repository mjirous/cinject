cmake_minimum_required (VERSION 3.0)

SET (PROJECTNAME cinject_test_app)
project (${PROJECTNAME})

SET (SRCS
    ${CMAKE_SOURCE_DIR}/include/cinject/cinject.h
    cinject_test_app.cpp
)

add_executable (${PROJECTNAME} ${SRCS} ${ALL_HEADERS})
