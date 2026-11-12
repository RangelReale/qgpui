# qgpui_add_qt_test(<name> SOURCES <files...> LIBRARIES <targets...>)
#
# Builds a Qt Test executable and registers it with CTest. Tests log in TAP because Qt Test's
# plain-text logger prints nothing when stdout is a pipe on Windows. CTest puts Qt's DLLs on PATH
# and selects the offscreen platform plugin, so run tests through CTest.
function(qgpui_add_qt_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBRARIES" ${ARGN})
    qt_add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBRARIES})
    add_test(NAME ${name} COMMAND ${name} -o -,tap)
    if(WIN32)
        set_tests_properties(${name} PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
    endif()
    set_property(TEST ${name} APPEND PROPERTY ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen"
        "QT_PLUGIN_PATH=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}")
endfunction()
