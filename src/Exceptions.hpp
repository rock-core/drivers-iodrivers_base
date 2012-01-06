#ifndef IODRIVERS_BASE_EXCEPTIONS_HPP
#define IODRIVERS_BASE_EXCEPTIONS_HPP

#include <string>
#include <stdexcept>

namespace iodrivers_base
{

/** Exception raised when a unix error occured in readPacket or writePacket
 */
struct UnixError : std::runtime_error
{
    int const error;
    explicit UnixError(std::string const& desc);

    UnixError(std::string const& desc, int error_code);
};

/** Exception raised when a timeout occured in readPacket or writePacket */
struct TimeoutError : std::runtime_error
{
    enum TIMEOUT_TYPE
    { NONE, PACKET, FIRST_BYTE };

    TIMEOUT_TYPE type;

    explicit TimeoutError(TIMEOUT_TYPE type, std::string const& desc)
        : std::runtime_error(desc)
        , type(type) {}
};

}

#endif

