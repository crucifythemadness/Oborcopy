cmake_minimum_required(VERSION 3.21)

project(Oborcopy
    VERSION 1.0.0
    DESCRIPTION "GUI file synchronisation and backup tool"
    LANGUAGES CXX)

if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 6.4 is the minimum because Ubuntu 24.04 (Linux build container) ships it.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets)
qt_standard_project_setup()

option(OBORCOPY_BUILD_TESTS "Build the unit and integration tests" ON)

set(OBORCOPY_GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
configure_file(cmake/version.h.in ${OBORCOPY_GENERATED_DIR}/version.h @ONLY)

# Regenerated on every build so the About dialog always shows the real build date.
add_custom_target(oborcopy_buildinfo
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT=${OBORCOPY_GENERATED_DIR}/buildinfo.cpp
        -P ${CMAKE_SOURCE_DIR}/cmake/BuildInfo.cmake
    BYPRODUCTS ${OBORCOPY_GENERATED_DIR}/buildinfo.cpp
    COMMENT "Updating build date")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(src)

if(OBORCOPY_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()
    add_subdirectory(tests)
endif()
