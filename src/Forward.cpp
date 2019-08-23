#include <iodrivers_base/Forward.hpp>
#include <iodrivers_base/Driver.hpp>
#include <thread>
#include <vector>
#include <poll.h>

using namespace std;
using namespace iodrivers_base;

struct QuitGuard
{
    bool& quitFlag;
    Driver& driver1;
    Driver& driver2;

    ~QuitGuard() {
        quitFlag = true;
    }
};

typedef int (Driver::*ReadMode)(uint8_t*, int, base::Time const&);

static int forwardData(Driver& from, ReadMode mode, Driver& to,
                        uint8_t* buffer, int buffer_size, base::Time timeout)
{
    int size = 0;
    try {
        size = (from.*mode)(buffer, buffer_size, timeout);
    }
    catch(TimeoutError&) {
    }
    if (size != 0) {
        to.writePacket(buffer, size);
    }
    return size;
}

void iodrivers_base::forward(bool raw_mode,
                Driver& driver1, Driver& driver2,
                base::Time timeout1,
                base::Time timeout2,
                bool oneway,
                size_t const buffer_size)
{
    vector<uint8_t> buffer(buffer_size);

    pollfd fds[2] = {
        { driver1.getFileDescriptor(), POLLIN, 0 },
        { driver2.getFileDescriptor(), POLLIN, 0 }
    };
    int nfds = oneway ? 1 : 2;

    ReadMode readMode = raw_mode ? static_cast<ReadMode>(&Driver::readRaw) :
                                   static_cast<ReadMode>(&Driver::readPacket);

    while (!driver1.eof() && !driver2.eof()) {
        int ret = poll(fds, nfds, 10000);
        if (ret < 0 && errno != EINTR)
            throw UnixError("forward(): error in select()");
        else if (ret == 0)
            continue;

        if (fds[0].revents & POLLIN) {
            forwardData(driver1, readMode, driver2, &buffer[0], buffer_size, timeout1);
        }

        if (fds[1].revents & POLLIN) {
            forwardData(driver2, readMode, driver1, &buffer[0], buffer_size, timeout1);
        }

        if (fds[0].revents & POLLERR || fds[1].revents & POLLERR) {
            break;
        }
    }
}
