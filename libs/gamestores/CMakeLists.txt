# gamestores: finds games installed from Steam, GOG and Epic on this PC, and the GOG games the user owns.
qt_add_library(gamestores STATIC
    include/gamestores/FilesFolder.h
    include/gamestores/Finder.h
    include/gamestores/Epic.h
    include/gamestores/Gog.h
    include/gamestores/Steam.h
    include/gamestores/Vdf.h
    src/Epic.cpp
    src/FilesFolder.cpp
    src/Finder.cpp
    src/Gog.cpp
    src/SafePath.h
    src/Steam.cpp
    src/Vdf.cpp
)
add_library(qgpui::gamestores ALIAS gamestores)

target_include_directories(gamestores
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE src)
target_link_libraries(gamestores PUBLIC Qt6::Core PRIVATE Qt6::Sql)
target_compile_definitions(gamestores PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)

if(QGPUI_BUILD_TESTS)
    add_subdirectory(tests)
endif()
