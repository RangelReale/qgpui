cmake_minimum_required(VERSION 3.25)

project(qgpui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# The presets name Qt's folder below a root in aqtinstall's layout, for example 6.8.3/msvc2022_64.
# The root is the QT_ROOT environment variable, for a Qt folder that several projects share, or
# else .qt/ in the repository.
if(QGPUI_QT_SUBDIR)
    if(NOT "$ENV{QT_ROOT}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{QT_ROOT}" qgpui_qt_root)
    else()
        set(qgpui_qt_root "${CMAKE_SOURCE_DIR}/.qt")
    endif()
    list(PREPEND CMAKE_PREFIX_PATH "${qgpui_qt_root}/${QGPUI_QT_SUBDIR}")
endif()

option(QGPUI_BUILD_TESTS "Build the tests" ON)
option(QGPUI_BUILD_TOOLS "Build the developer tools in tools/" ON)
option(QGPUI_BUILD_APPS "Build the applications in apps/" ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network Sql)
if(QGPUI_BUILD_TESTS)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
endif()
if(QGPUI_BUILD_TOOLS OR QGPUI_BUILD_APPS)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
endif()
qt_standard_project_setup()

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(cmake/QgpuiTesting.cmake)

if(QGPUI_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(libs)

if(QGPUI_BUILD_APPS)
    add_subdirectory(apps)
endif()

if(QGPUI_BUILD_TOOLS)
    add_subdirectory(tools)
endif()
