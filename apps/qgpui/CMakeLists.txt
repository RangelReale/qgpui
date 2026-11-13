# qgpui: the main application. Shows the QuestPorts games from the catalog that
# tools/questports-payload builds (build-catalog).
qt_add_executable(qgpui WIN32 MACOSX_BUNDLE
    main.cpp
    MainWindow.h MainWindow.cpp
    GameListModel.h GameListModel.cpp
    GameSortModel.h GameSortModel.cpp
    CoverCache.h CoverCache.cpp
    HeadsetConnection.h HeadsetConnection.cpp
    HeadsetDialog.h HeadsetDialog.cpp
    GameDialog.h GameDialog.cpp
    GuideView.h GuideView.cpp
    InputResolver.h InputResolver.cpp
    GameFlags.h GameFlags.cpp
    SettingsDialog.h SettingsDialog.cpp
    SingleInstance.h SingleInstance.cpp
)
if(WIN32)
    target_sources(qgpui PRIVATE qgpui.rc)
endif()

# The window icon. The icons are temporary; icons/make-icon.ps1 draws them.
qt_add_resources(qgpui icons
    PREFIX /
    FILES
        icons/qgpui-16.png
        icons/qgpui-24.png
        icons/qgpui-32.png
        icons/qgpui-48.png
        icons/qgpui-64.png
        icons/qgpui-128.png
        icons/qgpui-256.png
)

target_link_libraries(qgpui PRIVATE qgpui::questports qgpui::gamestores qgpui::installer qgpui::qadb Qt6::Widgets Qt6::Network)
target_compile_definitions(qgpui PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII
    QGPUI_VERSION="${PROJECT_VERSION}")

# Development builds fall back to the catalog in the repository's local/ folder.
option(QGPUI_DEV_CATALOG "Fall back to local/qgpui-catalog.json in the source tree" ON)
if(QGPUI_DEV_CATALOG)
    target_compile_definitions(qgpui PRIVATE
        QGPUI_DEV_CATALOG="${PROJECT_SOURCE_DIR}/local/qgpui-catalog.json")
endif()

# Copy the Qt runtime next to the executable so it runs from the build tree.
if(WIN32)
    add_custom_command(TARGET qgpui POST_BUILD
        COMMAND $<TARGET_FILE:Qt6::windeployqt> --no-translations --no-compiler-runtime
                --no-system-d3d-compiler --no-opengl-sw $<TARGET_FILE:qgpui>
        VERBATIM)
endif()

# Ship adb in platform-tools/ next to the executable, so users need no Android SDK.
# qadb::AdbServer::findAdb() looks there. QGPUI_PLATFORM_TOOLS names an unpacked platform-tools
# folder (the Android SDK's by default); set it to an empty string to ship no adb.
if(WIN32)
    file(TO_CMAKE_PATH "$ENV{LOCALAPPDATA}/Android/Sdk/platform-tools" _qgpui_default_tools)
    set(_qgpui_adb_files adb.exe AdbWinApi.dll AdbWinUsbApi.dll NOTICE.txt)
elseif(APPLE)
    set(_qgpui_default_tools "$ENV{HOME}/Library/Android/sdk/platform-tools")
    set(_qgpui_adb_files adb NOTICE.txt)
else()
    set(_qgpui_default_tools "$ENV{HOME}/Android/Sdk/platform-tools")
    set(_qgpui_adb_files adb NOTICE.txt)
endif()
set(QGPUI_PLATFORM_TOOLS "${_qgpui_default_tools}" CACHE PATH
    "Android platform-tools folder whose adb is copied next to qgpui; empty to ship none")

if(QGPUI_PLATFORM_TOOLS)
    set(_qgpui_adb_paths)
    foreach(_file IN LISTS _qgpui_adb_files)
        if(NOT EXISTS "${QGPUI_PLATFORM_TOOLS}/${_file}")
            message(WARNING "qgpui: ${QGPUI_PLATFORM_TOOLS}/${_file} not found; qgpui ships without adb. "
                            "Set QGPUI_PLATFORM_TOOLS to an unpacked platform-tools folder.")
            set(_qgpui_adb_paths)
            break()
        endif()
        list(APPEND _qgpui_adb_paths "${QGPUI_PLATFORM_TOOLS}/${_file}")
    endforeach()
    if(_qgpui_adb_paths)
        add_custom_command(TARGET qgpui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:qgpui>/platform-tools
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_qgpui_adb_paths}
                    $<TARGET_FILE_DIR:qgpui>/platform-tools
            VERBATIM)
    endif()
endif()

# `cmake --install` lays out the app as it ships: qgpui.exe with the Qt and MSVC runtimes beside it
# and adb in platform-tools/. packaging/windows/qgpui.iss builds the installer from that folder.
if(WIN32)
    install(TARGETS qgpui RUNTIME DESTINATION .)
    install(CODE "
        execute_process(
            COMMAND \"$<TARGET_FILE:Qt6::windeployqt>\" --no-translations --no-compiler-runtime
                    --no-system-d3d-compiler --no-opengl-sw \"\${CMAKE_INSTALL_PREFIX}/qgpui.exe\"
            COMMAND_ERROR_IS_FATAL ANY)
    ")
    if(_qgpui_adb_paths)
        install(FILES ${_qgpui_adb_paths} DESTINATION platform-tools)
    endif()
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    set(CMAKE_INSTALL_UCRT_LIBRARIES OFF)
    include(InstallRequiredSystemLibraries)
endif()
