cmake_minimum_required (VERSION 3.0)

project("cinject")

include(CMake-ExternalProjects.cmake)

option(BUILD_TESTS "Build tests" OFF)

set(CMAKE_CXX_STANDARD 11)

if (UNIX OR MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif (UNIX OR MINGW)

file (GLOB_RECURSE ALL_HEADERS ${PROJECT_SOURCE_DIR}/include/*.h)

include_directories("${PROJECT_SOURCE_DIR}/include")

add_subdirectory (src/sample)

if (BUILD_TESTS)
	message("BUILD_TESTS is ON, Tests will be included.")
	enable_testing()
	add_subdirectory (src/test)
endif (BUILD_TESTS)

install(FILES ${PROJECT_SOURCE_DIR}/include/cinject/cinject.h DESTINATION include)
