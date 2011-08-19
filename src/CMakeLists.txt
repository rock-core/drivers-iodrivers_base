rock_library(iodrivers_base
    SOURCES Driver.cpp Bus.cpp
    HEADERS Driver.hpp Bus.hpp)

# For backward compatibility only
install(FILES iodrivers_base.hh iodrivers_bus.hh
    DESTINATION include)

