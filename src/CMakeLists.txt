find_package(Boost COMPONENTS thread system regex)
include_directories(${Boost_INCLUDE_DIRS})

find_package(Threads REQUIRED)

rock_library(iodrivers_base
    SOURCES Driver.cpp Bus.cpp Timeout.cpp IOStream.cpp Exceptions.cpp TCPDriver.cpp
    IOListener.cpp TestStream.cpp Forward.cpp URI.cpp
    HEADERS Driver.hpp Bus.hpp Timeout.hpp Status.hpp IOStream.hpp
    Exceptions.hpp IOListener.hpp TCPDriver.hpp TestStream.hpp URI.hpp
    Fixture.hpp FixtureBoostTest.hpp FixtureGTest.hpp Forward.hpp SerialConfiguration.hpp
    LIBS ${Boost_THREAD_LIBRARY}
         ${Boost_SYSTEM_LIBRARY}
         ${Boost_REGEX_LIBRARY}
         ${CMAKE_THREAD_LIBS_INIT}
    DEPS_PKGCONFIG base-types base-lib)

rock_executable(iodrivers_base_cat
    SOURCES MainCat.cpp
    DEPS iodrivers_base)

rock_executable(iodrivers_base_forwarder
    SOURCES MainForwarder.cpp
    DEPS iodrivers_base)

# For backward compatibility only
install(FILES iodrivers_base.hh iodrivers_bus.hh
    DESTINATION include)

