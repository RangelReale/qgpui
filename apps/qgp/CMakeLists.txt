# qgp: the main application. Shows the QuestPorts games from the catalog that
# tools/questports-payload builds (build-catalog).
qt_add_executable(qgp WIN32 MACOSX_BUNDLE
    main.cpp
    MainWindow.h MainWindow.cpp
    GameListModel.h GameListModel.cpp
    GameSortModel.h GameSortModel.cpp
    CoverCache.h CoverCache.cpp
    HeadsetConnection.h HeadsetConnection.cpp
    HeadsetDialog.h HeadsetDialog.cpp
    GameDialog.h GameDialog.cpp
    InputResolver.h InputResolver.cpp
    GameFlags.h GameFlags.cpp
    SettingsDialog.h SettingsDialog.cpp
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

target_link_libraries(qgp PRIVATE qgp::questports qgp::gamestores qgp::installer qgp::qadb Qt6::Widgets Qt6::Network)
target_compile_definitions(qgp PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)

# Development builds fall back to the catalog in the repository's local/ folder.
option(QGP_DEV_CATALOG "Fall back to local/qgp-catalog.json in the source tree" ON)
if(QGP_DEV_CATALOG)
    target_compile_definitions(qgp PRIVATE
        QGP_DEV_CATALOG="${PROJECT_SOURCE_DIR}/local/qgp-catalog.json")
endif()

# Copy the Qt runtime next to the executable so it runs from the build tree.
if(WIN32)
    add_custom_command(TARGET qgp POST_BUILD
        COMMAND $<TARGET_FILE:Qt6::windeployqt> --no-translations --no-compiler-runtime
                --no-system-d3d-compiler --no-opengl-sw $<TARGET_FILE:qgp>
        VERBATIM)
endif()

# Ship adb in platform-tools/ next to the executable, so users need no Android SDK.
# qadb::AdbServer::findAdb() looks there. QGP_PLATFORM_TOOLS names an unpacked platform-tools
# folder (the Android SDK's by default); set it to an empty string to ship no adb.
if(WIN32)
    file(TO_CMAKE_PATH "$ENV{LOCALAPPDATA}/Android/Sdk/platform-tools" _qgp_default_tools)
    set(_qgp_adb_files adb.exe AdbWinApi.dll AdbWinUsbApi.dll NOTICE.txt)
elseif(APPLE)
    set(_qgp_default_tools "$ENV{HOME}/Library/Android/sdk/platform-tools")
    set(_qgp_adb_files adb NOTICE.txt)
else()
    set(_qgp_default_tools "$ENV{HOME}/Android/Sdk/platform-tools")
    set(_qgp_adb_files adb NOTICE.txt)
endif()
set(QGP_PLATFORM_TOOLS "${_qgp_default_tools}" CACHE PATH
    "Android platform-tools folder whose adb is copied next to qgp; empty to ship none")

if(QGP_PLATFORM_TOOLS)
    set(_qgp_adb_paths)
    foreach(_file IN LISTS _qgp_adb_files)
        if(NOT EXISTS "${QGP_PLATFORM_TOOLS}/${_file}")
            message(WARNING "qgp: ${QGP_PLATFORM_TOOLS}/${_file} not found; qgp ships without adb. "
                            "Set QGP_PLATFORM_TOOLS to an unpacked platform-tools folder.")
            set(_qgp_adb_paths)
            break()
        endif()
        list(APPEND _qgp_adb_paths "${QGP_PLATFORM_TOOLS}/${_file}")
    endforeach()
    if(_qgp_adb_paths)
        add_custom_command(TARGET qgp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:qgp>/platform-tools
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_qgp_adb_paths}
                    $<TARGET_FILE_DIR:qgp>/platform-tools
            VERBATIM)
    endif()
endif()
