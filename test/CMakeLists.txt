rock_testsuite(test_suite suite.cpp
    test_Driver.cpp test_TestStream.cpp test_Forward.cpp test_URI.cpp
    test_SerialConfiguration.cpp
    DEPS iodrivers_base)

rock_gtest(test_TestStreamGTest
   test_TestStreamGTest.cpp
   HEADERS
   DEPS iodrivers_base)

rock_executable(test_tcp_read test_tcp_read.cpp
    DEPS iodrivers_base
    NOINSTALL)
rock_executable(test_tcp_write test_tcp_write.cpp
    DEPS iodrivers_base
    NOINSTALL)

rock_executable(test_tcpserver_read test_tcpserver_read.cpp
    DEPS iodrivers_base
    NOINSTALL)        

rock_executable(test_tcpserver_write test_tcpserver_write.cpp
    DEPS iodrivers_base
    NOINSTALL)            

rock_executable(test_udp_read test_udp_read.cpp
    DEPS iodrivers_base
    NOINSTALL)
rock_executable(test_udp_write test_udp_write.cpp
    DEPS iodrivers_base
    NOINSTALL)


