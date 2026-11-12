qgpui_add_qt_test(tst_qadb_protocol
    SOURCES tst_protocol.cpp
    LIBRARIES qgpui::qadb)

qgpui_add_qt_test(tst_qadb_client
    SOURCES tst_client.cpp FakeAdbServer.h FakeAdbServer.cpp
    LIBRARIES qgpui::qadb)

# Against the real adb server and devices; skipped unless QADB_LIVE or QADB_LIVE_SERIAL is set.
# See the comment at the top of tst_live.cpp.
qgpui_add_qt_test(tst_qadb_live
    SOURCES tst_live.cpp
    LIBRARIES qgpui::qadb)
