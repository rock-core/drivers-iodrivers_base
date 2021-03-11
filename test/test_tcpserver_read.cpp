#include <iodrivers_base/Driver.hpp>
#include <iostream>
#include <string.h>

using namespace iodrivers_base;
using std::string;

struct DisplayDriver : public iodrivers_base::Driver
{
    DisplayDriver()
        : iodrivers_base::Driver(10000) {}
    int extractPacket(uint8_t const* buffer, size_t size) const
    {
        std::cout << iodrivers_base::Driver::printable_com(buffer, size) << std::endl;
        return -size;
    }
};

int main(int argc, char const* const* argv)
{
    if (argc < 2)
        throw UnixError("to few arguments, add tcp server port");

    string addr = string("tcpserver://localhost:") + argv[1];
    std::cout << "TCP server: " << addr << std::endl;

    DisplayDriver driver;
    driver.openURI(addr);

    uint8_t buffer[10000];
    driver.setReadTimeout(base::Time::fromSeconds(60));
    
    driver.readPacket(buffer, 10000);
    return 0;
}

