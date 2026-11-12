# questports: reads the qgpui catalog that tools/questports-payload builds (build-catalog).
qt_add_library(questports STATIC
    include/questports/Catalog.h
    include/questports/GameOrder.h
    src/Catalog.cpp
    src/GameOrder.cpp
)
add_library(qgpui::questports ALIAS questports)

target_include_directories(questports
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE src)
target_link_libraries(questports PUBLIC Qt6::Core)
target_compile_definitions(questports PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)

if(QGPUI_BUILD_TESTS)
    add_subdirectory(tests)
endif()
