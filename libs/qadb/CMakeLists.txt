# qadb: a Qt client for the adb server.
qt_add_library(qadb STATIC
    include/qadb/qadb.h
    include/qadb/AdbServer.h
    include/qadb/Client.h
    include/qadb/DeviceTracker.h
    include/qadb/Protocol.h
    include/qadb/Reply.h
    include/qadb/Types.h
    src/AdbServer.cpp
    src/Channel.h src/Channel.cpp
    src/Client.cpp
    src/DeviceTracker.cpp
    src/Protocol.cpp
    src/Reply.cpp
    src/ReplyAccess.h
    src/Types.cpp
)
add_library(qgpui::qadb ALIAS qadb)

target_include_directories(qadb
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE src)
target_link_libraries(qadb PUBLIC Qt6::Core Qt6::Network)
target_compile_definitions(qadb PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)

if(QGPUI_BUILD_TESTS)
    add_subdirectory(tests)
endif()
