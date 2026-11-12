qgp_add_qt_test(tst_gamestores_vdf
    SOURCES tst_vdf.cpp
    LIBRARIES qgp::gamestores)

qgp_add_qt_test(tst_gamestores_steam
    SOURCES tst_steam.cpp
    LIBRARIES qgp::gamestores)

qgp_add_qt_test(tst_gamestores_gog
    SOURCES tst_gog.cpp
    LIBRARIES qgp::gamestores Qt6::Sql)

qgp_add_qt_test(tst_gamestores_filesfolder
    SOURCES tst_filesfolder.cpp
    LIBRARIES qgp::gamestores)

qgp_add_qt_test(tst_gamestores_finder
    SOURCES tst_finder.cpp
    LIBRARIES qgp::gamestores)

# Reads the real Steam and GOG libraries of this PC and matches every recipe against them. Skipped
# unless QGP_STORES_LIVE=1.
qgp_add_qt_test(tst_gamestores_live
    SOURCES tst_live.cpp
    LIBRARIES qgp::gamestores)
target_compile_definitions(tst_gamestores_live PRIVATE
    QGP_RECIPES_DIR="${PROJECT_SOURCE_DIR}/data/recipes")
