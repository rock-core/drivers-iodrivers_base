rock_library(iodrivers_base
    SOURCES Driver.cpp Bus.cpp Timeout.cpp
    HEADERS Driver.hpp Bus.hpp Timeout.hpp Status.hpp)

# For backward compatibility only
install(FILES iodrivers_base.hh iodrivers_bus.hh
    DESTINATION include)

