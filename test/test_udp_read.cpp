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
    DisplayDriver driver;
    driver.openURI(string("udpserver://") + argv[1]);

    uint8_t buffer[10000];
    driver.setReadTimeout(base::Time::fromSeconds(3));
    driver.readPacket(buffer, 10000);
    return 0;
}

