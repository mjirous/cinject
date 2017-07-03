cmake_minimum_required (VERSION 3.0)

project("cinject")


option(BUILD_TESTS "Build tests" OFF)

if (UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
endif (UNIX)



include_directories("${PROJECT_SOURCE_DIR}/include")

add_subdirectory (src)

if (BUILD_TESTS)
	message("BUILD_TESTS is ON, Tests will be included.")
	enable_testing()
	find_package(GTest REQUIRED)
    find_package (Threads REQUIRED)

	include_directories(${GTEST_INCLUDE_DIRS})
	
	add_subdirectory (test)
endif (BUILD_TESTS)