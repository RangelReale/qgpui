qgp_add_qt_test(tst_installer_recipe
    SOURCES tst_recipe.cpp
    LIBRARIES qgp::installer)

qgp_add_qt_test(tst_installer_pushplan
    SOURCES tst_pushplan.cpp
    LIBRARIES qgp::installer)

qgp_add_qt_test(tst_installer_updates
    SOURCES tst_updates.cpp
    LIBRARIES qgp::installer)

qgp_add_qt_test(tst_installer_releases
    SOURCES tst_releases.cpp
    LIBRARIES qgp::installer Qt6::Network)

# Runs recipes against qadb's FakeAdbServer.
qgp_add_qt_test(tst_installer_runner
    SOURCES tst_runner.cpp
            ${PROJECT_SOURCE_DIR}/libs/qadb/tests/FakeAdbServer.h
            ${PROJECT_SOURCE_DIR}/libs/qadb/tests/FakeAdbServer.cpp
    LIBRARIES qgp::installer Qt6::Network)
target_include_directories(tst_installer_runner PRIVATE ${PROJECT_SOURCE_DIR}/libs/qadb/tests)

qgp_add_qt_test(tst_installer_headsetrecords
    SOURCES tst_headsetrecords.cpp
            ${PROJECT_SOURCE_DIR}/libs/qadb/tests/FakeAdbServer.h
            ${PROJECT_SOURCE_DIR}/libs/qadb/tests/FakeAdbServer.cpp
    LIBRARIES qgp::installer Qt6::Network)
target_include_directories(tst_installer_headsetrecords PRIVATE ${PROJECT_SOURCE_DIR}/libs/qadb/tests)

qgp_add_qt_test(tst_installer_keepawake
    SOURCES tst_keepawake.cpp
            ${PROJECT_SOURCE_DIR}/libs/qadb/tests/FakeAdbServer.h
            ${PROJECT_SOURCE_DIR}/libs/qadb/tests/FakeAdbServer.cpp
    LIBRARIES qgp::installer Qt6::Network)
target_include_directories(tst_installer_keepawake PRIVATE ${PROJECT_SOURCE_DIR}/libs/qadb/tests)

# Installs one recipe on a real headset. Skipped unless QGP_INSTALL_LIVE_SERIAL and
# QGP_INSTALL_LIVE_SLUG are set; see tst_live.cpp.
qgp_add_qt_test(tst_installer_live
    SOURCES tst_live.cpp
    LIBRARIES qgp::installer)
target_compile_definitions(tst_installer_live PRIVATE QGP_RECIPES_DIR="${PROJECT_SOURCE_DIR}/data/recipes")
