include(ExternalProject)

add_custom_target(_BEFORE_ALL)

if (BUILD_TESTS)
    find_package (Threads REQUIRED)

    if (NOT GTEST_ROOT)
    
        if (MSVC)
            set(FORCE_SHARED_CRT "-Dgtest_force_shared_crt=ON")
        endif()
    
        ExternalProject_Add(
                GoogleTest

                GIT_REPOSITORY "https://github.com/google/googletest.git"
                GIT_TAG "master"

                UPDATE_COMMAND ""
                PATCH_COMMAND ""

                SOURCE_DIR "${CMAKE_SOURCE_DIR}/.3rdparty/gtest"
                CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DBUILD_GTEST=ON -DBUILD_GMOCK=ON -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/gtest ${FORCE_SHARED_CRT}

                TEST_COMMAND ""
        )

        set(GTEST_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/gtest/include)
        set(GTEST_BOTH_LIBRARIES gtest gtest_main)
        link_directories(${CMAKE_BINARY_DIR}/gtest/lib)
    else()
        find_package (GTest REQUIRED)
        add_library(GoogleTest INTERFACE)
    endif()

    include_directories(${GTEST_INCLUDE_DIRS})
	add_dependencies(_BEFORE_ALL GoogleTest)
else()
    message("BUILD_TESTS is OFF")
endif()
