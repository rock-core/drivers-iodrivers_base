#include <iodrivers_base/Forward.hpp>
#include <iodrivers_base/Driver.hpp>
#include <iostream>
#include <thread>

using namespace std;
using namespace iodrivers_base;

static void usage(ostream& out) {
    out << "iodrivers_base_forwarder URI1 TIMEOUT1 URI2 TIMEOUT2\n"
        << "  forwards data (two-way) between URI1 and URI2, which must both\n"
        << "  be valid iodrivers_base URIs\n"
        << "\n"
        << "  TIMEOUT1 and TIMEOUT2 define how long (in milliseconds) the forwarder should\n"
        << "  wait on read before forwarding the data, to avoid unnecessary fragmentation\n"
        << flush;
}

static const int BUFFER_SIZE = 32768;

class RawIODriver : public iodrivers_base::Driver {
    int extractPacket(uint8_t const* buffer, size_t buffer_size) const {
        return 0;
    }
public:
    RawIODriver()
        : Driver(BUFFER_SIZE) {}
};

int main(int argc, char** argv) {
    if (argc != 5) {
        usage(argc == 1 ? cout : cerr);
        return argc == 1 ? 0 : 1;
    }

    string uri1 = argv[1];
    base::Time timeout1 = base::Time::fromMilliseconds(atoi(argv[2]));
    string uri2 = argv[3];
    base::Time timeout2 = base::Time::fromMilliseconds(atoi(argv[4]));

    while(true) {
        RawIODriver driver1;
        driver1.openURI(uri1);
        RawIODriver driver2;
        driver2.openURI(uri2);

        forward(true, driver1, driver2, timeout1, timeout2, BUFFER_SIZE);
    }
    return 0;
}
