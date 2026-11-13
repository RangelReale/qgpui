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

# macOS: the bundle's Info.plist, and qgpui.icns made from icons/macos/'s PNGs with iconutil.
if(APPLE)
    set(_qgpui_iconset "${CMAKE_CURRENT_BINARY_DIR}/qgpui.iconset")
    set(_qgpui_icns "${CMAKE_CURRENT_BINARY_DIR}/qgpui.icns")
    set(_qgpui_iconset_commands)
    # iconutil's names: icon_<size>x<size>[@2x].png, where @2x holds twice the pixels.
    foreach(_pair IN ITEMS 16:16 32:16@2x 32:32 64:32@2x 128:128 256:128@2x 256:256 512:256@2x
                           512:512 1024:512@2x)
        string(REPLACE ":" ";" _pair "${_pair}")
        list(GET _pair 0 _pixels)
        list(GET _pair 1 _name)
        string(REGEX REPLACE "@2x$" "" _points "${_name}")
        string(REGEX MATCH "@2x$" _scale "${_name}")
        list(APPEND _qgpui_iconset_commands COMMAND ${CMAKE_COMMAND} -E copy
             "${CMAKE_CURRENT_SOURCE_DIR}/icons/macos/qgpui-${_pixels}.png"
             "${_qgpui_iconset}/icon_${_points}x${_points}${_scale}.png")
    endforeach()
    add_custom_command(OUTPUT "${_qgpui_icns}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${_qgpui_iconset}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_qgpui_iconset}"
        ${_qgpui_iconset_commands}
        COMMAND iconutil --convert icns --output "${_qgpui_icns}" "${_qgpui_iconset}"
        DEPENDS icons/macos/qgpui-16.png icons/macos/qgpui-32.png icons/macos/qgpui-64.png
                icons/macos/qgpui-128.png icons/macos/qgpui-256.png icons/macos/qgpui-512.png
                icons/macos/qgpui-1024.png
        VERBATIM)
    target_sources(qgpui PRIVATE "${_qgpui_icns}")
    set_source_files_properties("${_qgpui_icns}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(qgpui PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "QGPUI"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.rangelreale.qgpui"
        MACOSX_BUNDLE_ICON_FILE "qgpui.icns"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
endif()

# The window icon. The icons are temporary; icons/make-icon.ps1 draws them. On macOS the window
# icon is also the Dock icon, so it comes from icons/macos/, which icons/macos/make-icons.sh draws
# up to 1024 pixels on Apple's icon grid.
if(APPLE)
    qt_add_resources(qgpui icons
        PREFIX /icons
        BASE icons/macos
        FILES
            icons/macos/qgpui-16.png
            icons/macos/qgpui-32.png
            icons/macos/qgpui-64.png
            icons/macos/qgpui-128.png
            icons/macos/qgpui-256.png
            icons/macos/qgpui-512.png
            icons/macos/qgpui-1024.png
    )
else()
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
endif()

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
    if(_qgpui_adb_paths AND APPLE)
        # A bundle's Contents/MacOS may hold only code, or codesign fails: NOTICE.txt goes to
        # Contents/Resources/platform-tools.
        add_custom_command(TARGET qgpui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:qgpui>/platform-tools
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QGPUI_PLATFORM_TOOLS}/adb"
                    $<TARGET_FILE_DIR:qgpui>/platform-tools
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    $<TARGET_BUNDLE_CONTENT_DIR:qgpui>/Resources/platform-tools
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QGPUI_PLATFORM_TOOLS}/NOTICE.txt"
                    $<TARGET_BUNDLE_CONTENT_DIR:qgpui>/Resources/platform-tools
            VERBATIM)
    elseif(_qgpui_adb_paths)
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

# macOS: the qgpui-bundle target makes a copy of the app that runs on other Macs, in
# <build>/bundle/<config>: qgpui.app with Qt inside it (macdeployqt), signed, and qgpui.dmg.
# Normal builds skip this step and find Qt through their RPATH (decision 0020).
if(APPLE)
    set(QGPUI_MACOS_SIGN_IDENTITY "-" CACHE STRING
        "Certificate that signs the qgpui-bundle app; - signs ad hoc, which only this Mac trusts")
    if(NOT TARGET Qt6::macdeployqt)
        message(WARNING "qgpui: macdeployqt was not found, so there is no qgpui-bundle target.")
    else()
        set(_qgpui_bundle_dir "${CMAKE_BINARY_DIR}/bundle/$<CONFIG>")
        add_custom_target(qgpui-bundle
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/make-bundle.sh" "$<TARGET_FILE:Qt6::macdeployqt>"
                    "$<TARGET_BUNDLE_DIR:qgpui>" "${_qgpui_bundle_dir}" "${QGPUI_MACOS_SIGN_IDENTITY}"
            DEPENDS qgpui
            COMMENT "Making a self-contained qgpui.app and qgpui.dmg in ${_qgpui_bundle_dir}"
            VERBATIM)
    endif()
endif()
