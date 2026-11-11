qgp_add_qt_test(tst_qadb_protocol
    SOURCES tst_protocol.cpp
    LIBRARIES qgp::qadb)

qgp_add_qt_test(tst_qadb_client
    SOURCES tst_client.cpp FakeAdbServer.h FakeAdbServer.cpp
    LIBRARIES qgp::qadb)
