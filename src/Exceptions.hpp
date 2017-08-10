#ifndef IODRIVERS_BASE_EXCEPTIONS_HPP
#define IODRIVERS_BASE_EXCEPTIONS_HPP

#include <string>
#include <stdexcept>
#include <exception>  

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
class MockContextException : public std::exception
{  
    public:  
        const char * what() const throw()  
        {  
            return "IODRIVERS_BASE_MOCK Error: Expectation set outside Mock Context! Please call IODRIVERS_BASE_MOCK() before setting expectations";
        }  
};  

class TestEndsWithExpectationsLeftException : public std::exception
{
       public:  
        const char * what() const throw()  
        {  
            return "IODRIVERS_BASE_MOCK Error: Test reached its end without satisfying all expecations";
        }  
};  
    
#endif

