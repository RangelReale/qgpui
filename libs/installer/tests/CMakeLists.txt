qgpui_add_qt_test(tst_installer_recipe
    SOURCES tst_recipe.cpp
    LIBRARIES qgpui::installer)
target_compile_definitions(tst_installer_recipe PRIVATE QGPUI_RECIPES_DIR="${PROJECT_SOURCE_DIR}/data/recipes")

qgpui_add_qt_test(tst_installer_pushplan
    SOURCES tst_pushplan.cpp
    LIBRARIES qgpui::installer)

qgpui_add_qt_test(tst_installer_updates
    SOURCES tst_updates.cpp
    LIBRARIES qgpui::installer)

qgpui_add_qt_test(tst_installer_releases
    SOURCES tst_releases.cpp
    LIBRARIES qgpui::installer Qt6::Network)

# Runs recipes against qadb's FakeAdbServer.
qgpui_add_qt_test(tst_installer_runner
    SOURCES tst_runner.cpp
            ${PROJECT_SOURCE_DIR}/libs/qadb/tests/FakeAdbServer.h
            ${PROJECT_SOURCE_DIR}/libs/qadb/tests/FakeAdbServer.cpp
    LIBRARIES qgpui::installer Qt6::Network)
target_include_directories(tst_installer_runner PRIVATE ${PROJECT_SOURCE_DIR}/libs/qadb/tests)

qgpui_add_qt_test(tst_installer_headsetrecords
    SOURCES tst_headsetrecords.cpp
            ${PROJECT_SOURCE_DIR}/libs/qadb/tests/FakeAdbServer.h
            ${PROJECT_SOURCE_DIR}/libs/qadb/tests/FakeAdbServer.cpp
    LIBRARIES qgpui::installer Qt6::Network)
target_include_directories(tst_installer_headsetrecords PRIVATE ${PROJECT_SOURCE_DIR}/libs/qadb/tests)

qgpui_add_qt_test(tst_installer_keepawake
    SOURCES tst_keepawake.cpp
            ${PROJECT_SOURCE_DIR}/libs/qadb/tests/FakeAdbServer.h
            ${PROJECT_SOURCE_DIR}/libs/qadb/tests/FakeAdbServer.cpp
    LIBRARIES qgpui::installer Qt6::Network)
target_include_directories(tst_installer_keepawake PRIVATE ${PROJECT_SOURCE_DIR}/libs/qadb/tests)

# Installs one recipe on a real headset. Skipped unless QGPUI_INSTALL_LIVE_SERIAL and
# QGPUI_INSTALL_LIVE_SLUG are set; see tst_live.cpp.
qgpui_add_qt_test(tst_installer_live
    SOURCES tst_live.cpp
    LIBRARIES qgpui::installer)
target_compile_definitions(tst_installer_live PRIVATE QGPUI_RECIPES_DIR="${PROJECT_SOURCE_DIR}/data/recipes")

qgpui_add_qt_test(tst_installer_installplan
    SOURCES tst_installplan.cpp
    LIBRARIES qgpui::installer)
