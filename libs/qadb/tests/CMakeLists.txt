qgp_add_qt_test(tst_qadb_protocol
    SOURCES tst_protocol.cpp
    LIBRARIES qgp::qadb)

qgp_add_qt_test(tst_qadb_client
    SOURCES tst_client.cpp FakeAdbServer.h FakeAdbServer.cpp
    LIBRARIES qgp::qadb)

# Against the real adb server and devices; skipped unless QADB_LIVE or QADB_LIVE_SERIAL is set.
# See the comment at the top of tst_live.cpp.
qgp_add_qt_test(tst_qadb_live
    SOURCES tst_live.cpp
    LIBRARIES qgp::qadb)
