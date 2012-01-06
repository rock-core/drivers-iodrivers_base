find_package(Boost COMPONENTS thread)
rock_library(iodrivers_base
    SOURCES Driver.cpp Bus.cpp Timeout.cpp IOStream.cpp Exceptions.cpp
    IOListener.cpp
    HEADERS Driver.hpp Bus.hpp Timeout.hpp Status.hpp IOStream.hpp
    Exceptions.hpp IOListener.hpp
    DEPS_CMAKE Boost
    DEPS_PKGCONFIG base-types base-lib)

# For backward compatibility only
install(FILES iodrivers_base.hh iodrivers_bus.hh
    DESTINATION include)

