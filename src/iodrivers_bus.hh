#warning "the iodrivers_base package has been moved to the Rock package layout"
#warning "and naming guidelines. iodrivers_base.hh is therefore deprecated"
#warning "  this header is now <iodrivers_base/Bus.hpp>"
#warning "  the classes are now:"
#warning "     IOBus => iodrivers_base::Bus"
#warning "     IOParser => iodrivers_base::Parser"
#warning "     IOBusHandler => iodrivers_base::BusHandler"
#warning "     unix_error => iodrivers_base::UnixError"
#warning "     timeout_error => iodrivers_base::TimeoutError"
#warning "     file_guard => iodrivers_base::FileGuard"
#include <iodrivers_base/Bus.hpp>
#include <iodrivers_base/Timeout.hpp>

typedef iodrivers_base::UnixError unix_error;
typedef iodrivers_base::FileGuard file_guard;
typedef iodrivers_base::TimeoutError timeout_error;
typedef iodrivers_base::Timeout Timeout;

typedef iodrivers_base::Bus IOBus;
typedef iodrivers_base::Parser IOParser;
typedef iodrivers_base::BusHandler IOBusHandler;

