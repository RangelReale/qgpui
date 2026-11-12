# adbmanager: a debug window for trying the qadb library against real devices. Not shipped.
qt_add_executable(adbmanager WIN32 MACOSX_BUNDLE
    main.cpp
    MainWindow.h MainWindow.cpp
)
target_link_libraries(adbmanager PRIVATE qgpui::qadb Qt6::Widgets)

# Copy the Qt runtime next to the executable so it runs from the build tree.
if(WIN32)
    add_custom_command(TARGET adbmanager POST_BUILD
        COMMAND $<TARGET_FILE:Qt6::windeployqt> --no-translations --no-compiler-runtime
                --no-system-d3d-compiler --no-opengl-sw $<TARGET_FILE:adbmanager>
        VERBATIM)
endif()
