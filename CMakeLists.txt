PROJECT(ioDriversBase)
SET(PROJECT_VERSION 1.0)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
INCLUDE(FindPkgConfig)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

ADD_LIBRARY(iodrivers_base SHARED iodrivers_base.cc)

INSTALL(TARGETS iodrivers_base LIBRARY DESTINATION lib)
INSTALL(FILES iodrivers_base.hh DESTINATION include)

CONFIGURE_FILE(Doxyfile.in Doxyfile @ONLY)
ADD_CUSTOM_TARGET(doc doxygen Doxyfile)

CONFIGURE_FILE(iodrivers_base.pc.in iodrivers_base.pc @ONLY)
INSTALL(FILES ${CMAKE_BINARY_DIR}/iodrivers_base.pc DESTINATION lib/pkgconfig)

FIND_PACKAGE(Boost COMPONENTS unit_test_framework)
IF (Boost_UNIT_TEST_FRAMEWORK_FOUND)
    MESSAGE(STATUS "boost/test found ... building test suite")

    ADD_EXECUTABLE(test test.cc)
    TARGET_LINK_LIBRARIES(test iodrivers_base ${Boost_UNIT_TEST_FRAMEWORK_LIBRARIES})
ENDIF(Boost_UNIT_TEST_FRAMEWORK_FOUND)


