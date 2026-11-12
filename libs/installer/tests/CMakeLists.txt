qgp_add_qt_test(tst_installer_recipe
    SOURCES tst_recipe.cpp
    LIBRARIES qgp::installer)

qgp_add_qt_test(tst_installer_pushplan
    SOURCES tst_pushplan.cpp
    LIBRARIES qgp::installer)

qgp_add_qt_test(tst_installer_releases
    SOURCES tst_releases.cpp
    LIBRARIES qgp::installer)

# Runs recipes against qadb's FakeAdbServer.
qgp_add_qt_test(tst_installer_runner
    SOURCES tst_runner.cpp
            ${PROJECT_SOURCE_DIR}/libs/qadb/tests/FakeAdbServer.h
            ${PROJECT_SOURCE_DIR}/libs/qadb/tests/FakeAdbServer.cpp
    LIBRARIES qgp::installer Qt6::Network)
target_include_directories(tst_installer_runner PRIVATE ${PROJECT_SOURCE_DIR}/libs/qadb/tests)
