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
    driver.openURI(string("udp://") + argv[1] + ":" + argv[2]);

    driver.setWriteTimeout(base::Time::fromSeconds(1));
    driver.writePacket(reinterpret_cast<uint8_t const*>(argv[3]), strlen(argv[3]));
    return 0;
}

