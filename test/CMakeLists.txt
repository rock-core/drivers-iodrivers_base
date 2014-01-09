rock_testsuite(test_suite test.cpp
    DEPS iodrivers_base)

rock_executable(test_tcp_read test_tcp_read.cpp
    DEPS iodrivers_base
    NOINSTALL)
rock_executable(test_tcp_write test_tcp_write.cpp
    DEPS iodrivers_base
    NOINSTALL)

rock_executable(test_udp_read test_udp_read.cpp
    DEPS iodrivers_base
    NOINSTALL)
rock_executable(test_udp_write test_udp_write.cpp
    DEPS iodrivers_base
    NOINSTALL)
