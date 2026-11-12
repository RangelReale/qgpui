# installer: runs install recipes on a headset: fetches APKs, plans the copies and runs the steps.
qt_add_library(installer STATIC
    include/installer/ApkFetcher.h
    include/installer/PushPlan.h
    include/installer/Recipe.h
    include/installer/Runner.h
    include/installer/Updates.h
    src/ApkFetcher.cpp
    src/PushPlan.cpp
    src/Recipe.cpp
    src/Runner.cpp
    src/Updates.cpp
)
add_library(qgp::installer ALIAS installer)

target_include_directories(installer
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE src)
target_link_libraries(installer PUBLIC Qt6::Core qgp::qadb PRIVATE Qt6::Network)
target_compile_definitions(installer PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)

if(QGP_BUILD_TESTS)
    add_subdirectory(tests)
endif()
