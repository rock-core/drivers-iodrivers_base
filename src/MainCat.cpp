#include <iodrivers_base/Driver.hpp>
#include <iostream>
#include <iomanip>
#include <thread>

using namespace std;
using namespace iodrivers_base;

static void usage(ostream& out) {
    out << "iodrivers_base_cat URI [TIMEOUT]\n"
        << "  displays data coming from a iodrivers_base-compatible URI"
        << "\n"
        << "  TIMEOUT defines how long (in milliseconds) the program should\n"
        << "  wait on read before displaying it. Defaults to 100ms\n"
        << flush;
}

static const int BUFFER_SIZE = 32768;
static const int COLUMN_SIZE = 8;
static const int LINE_SIZE = COLUMN_SIZE * 3;

class RawIODriver : public iodrivers_base::Driver {
    int extractPacket(uint8_t const* buffer, size_t buffer_size) const {
        return 0;
    }
public:
    RawIODriver()
        : Driver(BUFFER_SIZE) {}
};

void displayAscii(char* line) {
    for (int i = 0; i < LINE_SIZE; ++i) {
        if (i && i % COLUMN_SIZE == 0) {
            cout << " ";
        }

        cout << line[i];
    }
}

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) {
        usage(argc == 1 ? cout : cerr);
        return argc == 1 ? 0 : 1;
    }

    string uri = argv[1];
    int timeout_ms = 100;
    if (argc == 3) {
        timeout_ms = atoi(argv[2]);
    }

    base::Time timeout = base::Time::fromMilliseconds(timeout_ms);

    uint8_t buffer[BUFFER_SIZE];
    int pos = 0;
    char line[LINE_SIZE];

    while (true) {
        RawIODriver driver;
        driver.openURI(uri);

        while (true) {
            int count = driver.readRaw(buffer, BUFFER_SIZE, timeout);
            for (int i = 0; i < count; ++i) {
                if (pos) {
                    cout << " ";
                    if (pos % LINE_SIZE == 0) {
                        cout << "  ";
                        displayAscii(line);
                        cout << "\n";
                        pos = 0;
                    }
                    else if (pos % COLUMN_SIZE == 0) {
                        cout << "  ";
                    }
                }

                line[pos] = isprint(buffer[i]) ? buffer[i] : '.';
                cout << hex << setw(2) << setfill('0') << static_cast<int>(buffer[i]);
                ++pos;
            }
        }
    }
    return 0;
}

