rock_library(iodrivers_base
    SOURCES Driver.cpp Bus.cpp Timeout.cpp
    HEADERS Driver.hpp Bus.hpp Timeout.hpp Status.hpp
    DEPS_PKGCONFIG base-types)

# For backward compatibility only
install(FILES iodrivers_base.hh iodrivers_bus.hh
    DESTINATION include)

