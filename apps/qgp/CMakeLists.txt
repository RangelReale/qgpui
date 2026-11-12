# qgp: the main application. Shows the QuestPorts games from the payload that
# tools/questports-payload builds.
qt_add_executable(qgp WIN32 MACOSX_BUNDLE
    main.cpp
    MainWindow.h MainWindow.cpp
    GameListModel.h GameListModel.cpp
    CoverCache.h CoverCache.cpp
)
if(WIN32)
    target_sources(qgp PRIVATE qgp.rc)
endif()

# The window icon. The icons are temporary; icons/make-icon.ps1 draws them.
qt_add_resources(qgp icons
    PREFIX /
    FILES
        icons/qgp-16.png
        icons/qgp-24.png
        icons/qgp-32.png
        icons/qgp-48.png
        icons/qgp-64.png
        icons/qgp-128.png
        icons/qgp-256.png
)

target_link_libraries(qgp PRIVATE qgp::questports Qt6::Widgets Qt6::Network)
target_compile_definitions(qgp PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)

# Development builds fall back to the payload in the repository's local/ folder.
option(QGP_DEV_PAYLOAD "Fall back to local/questports-payload.json in the source tree" ON)
if(QGP_DEV_PAYLOAD)
    target_compile_definitions(qgp PRIVATE
        QGP_DEV_PAYLOAD="${PROJECT_SOURCE_DIR}/local/questports-payload.json")
endif()

# Copy the Qt runtime next to the executable so it runs from the build tree.
if(WIN32)
    add_custom_command(TARGET qgp POST_BUILD
        COMMAND $<TARGET_FILE:Qt6::windeployqt> --no-translations --no-compiler-runtime
                --no-system-d3d-compiler --no-opengl-sw $<TARGET_FILE:qgp>
        VERBATIM)
endif()
