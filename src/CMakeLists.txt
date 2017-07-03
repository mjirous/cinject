cmake_minimum_required (VERSION 3.0)

SET (PROJECTNAME cinject_test_app)
project (${PROJECTNAME})

SET (SRCS
    cinject_test_app.cpp
)

add_executable (${PROJECTNAME} ${SRCS})
