cmake_minimum_required(VERSION 3.25)

# macOS: Qt 6.8 and 6.9 draw widgets in the look macOS had before version 26. An app linked against
# the macOS 26 SDK or later gets macOS 26's controls, and Qt draws those wrongly: push buttons
# become flat rectangles. So build against the newest macOS 15 SDK that is installed (decision
# 0020). This must happen before project(). Set CMAKE_OSX_SYSROOT to choose another SDK.
if(CMAKE_HOST_APPLE AND NOT CMAKE_OSX_SYSROOT AND "$ENV{SDKROOT}" STREQUAL ""
   AND QGPUI_QT_SUBDIR MATCHES "^6\\.[89]\\.")
    execute_process(COMMAND xcrun --show-sdk-platform-path
                    OUTPUT_VARIABLE qgpui_sdk_platform OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    file(GLOB qgpui_sdks "${qgpui_sdk_platform}/Developer/SDKs/MacOSX15*.sdk"
                         "/Library/Developer/CommandLineTools/SDKs/MacOSX15*.sdk")
    list(SORT qgpui_sdks COMPARE NATURAL)
    if(qgpui_sdks)
        list(GET qgpui_sdks -1 qgpui_sdk)
        set(CMAKE_OSX_SYSROOT "${qgpui_sdk}" CACHE PATH "The macOS SDK to build against")
        message(STATUS "qgpui: building against ${qgpui_sdk}")
    else()
        message(WARNING "qgpui: no macOS 15 SDK found. With a newer SDK, Qt ${QGPUI_QT_SUBDIR} draws "
                        "flat push buttons. Install the Command Line Tools for Xcode 16.")
    endif()
endif()

project(qgpui VERSION 1.0.0 LANGUAGES CXX)

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

# Qt 6.8's FindWrapOpenGL links the AGL framework, which the macOS 26 SDK and later no longer
# have. Nothing here uses AGL, so drop it.
if(APPLE AND TARGET WrapOpenGL::WrapOpenGL)
    get_target_property(qgpui_gl_libs WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES)
    list(FILTER qgpui_gl_libs EXCLUDE REGEX "AGL")
    set_target_properties(WrapOpenGL::WrapOpenGL PROPERTIES INTERFACE_LINK_LIBRARIES "${qgpui_gl_libs}")
endif()

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(cmake/QgpuiTesting.cmake)

# qgpui and adbmanager run windeployqt after linking, and both copy Qt into the same bin folder.
# With Ninja the two could run at once and fail on the same file ("Cannot create a file when that
# file already exists"). Their links, which include those steps, share a pool of one.
set_property(GLOBAL APPEND PROPERTY JOB_POOLS qgpui_deploy=1)

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
