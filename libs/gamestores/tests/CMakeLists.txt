qgpui_add_qt_test(tst_gamestores_vdf
    SOURCES tst_vdf.cpp
    LIBRARIES qgpui::gamestores)

qgpui_add_qt_test(tst_gamestores_steam
    SOURCES tst_steam.cpp
    LIBRARIES qgpui::gamestores)

qgpui_add_qt_test(tst_gamestores_gog
    SOURCES tst_gog.cpp
    LIBRARIES qgpui::gamestores Qt6::Sql)

qgpui_add_qt_test(tst_gamestores_filesfolder
    SOURCES tst_filesfolder.cpp
    LIBRARIES qgpui::gamestores)

qgpui_add_qt_test(tst_gamestores_finder
    SOURCES tst_finder.cpp
    LIBRARIES qgpui::gamestores)

# Reads the real Steam and GOG libraries of this PC and matches every recipe against them. Skipped
# unless QGPUI_STORES_LIVE=1.
qgpui_add_qt_test(tst_gamestores_live
    SOURCES tst_live.cpp
    LIBRARIES qgpui::gamestores)
target_compile_definitions(tst_gamestores_live PRIVATE
    QGPUI_RECIPES_DIR="${PROJECT_SOURCE_DIR}/data/recipes")
