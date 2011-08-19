
#warning "the iodrivers_base package has been moved to the Rock package layout"
#warning "and naming guidelines. iodrivers_base.hh is therefore deprecated"
#warning "  the header is now <iodrivers_base/Driver.hpp>"
#warning "  the classes are now:"
#warning "     IODriver => iodrivers_base::Driver"
#warning "     unix_error => iodrivers_base::UnixError"
#warning "     timeout_error => iodrivers_base::TimeoutError"
#warning "     file_guard => iodrivers_base::FileGuard"

#include <iodrivers_base/Driver.hpp>
typedef iodrivers_base::UnixError unix_error;
typedef iodrivers_base::FileGuard file_guard;
typedef iodrivers_base::TimeoutError timeout_error;
typedef iodrivers_base::Driver IODriver;

