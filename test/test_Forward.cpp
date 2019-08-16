#include <boost/test/unit_test.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>

#include <iodrivers_base/Driver.hpp>
#include <iodrivers_base/Forward.hpp>
#include <iodrivers_base/IOStream.hpp>

using namespace std;
using namespace iodrivers_base;

/** Flow is
 *
 * RX/TX is defined w.r.t the forwarder. Flow of data is
 *
 * rxSockets[0] -> rxSockets[1] -> rxDriver -> txDriver -> txSockets[0] -> txSockets[1]
 */
template <typename Driver>
struct ForwardFixture {
    int rxSockets[2];
    Driver rxDriver;
    Driver txDriver;
    int txSockets[2];

    ForwardFixture() {
        socketpair(AF_UNIX, SOCK_STREAM, 0, rxSockets);
        socketpair(AF_UNIX, SOCK_STREAM, 0, txSockets);
        rxDriver.setFileDescriptor(rxSockets[1]);
        txDriver.setFileDescriptor(txSockets[0]);
    }

    ~ForwardFixture() {
        close(rxSockets[0]);
        close(txSockets[1]);
    }

    void write(uint8_t const* data, int size) {
        if (::write(rxSockets[0], data, size) != size) {
            throw std::runtime_error("failed writing the test data");
        }
    }

    int read(uint8_t* data, int size) {
        return ::read(txSockets[1], data, size);
    }
};

class RawForwardDriver : public Driver
{
public:
    RawForwardDriver() : Driver(100) {}
    int extractPacket(uint8_t const* buffer, size_t buffer_size) const
    {
        return 0;
    }
};

BOOST_FIXTURE_TEST_SUITE(ForwardSuite_RawMode, ForwardFixture<RawForwardDriver>)

BOOST_AUTO_TEST_CASE(it_quits_if_the_left_connection_closes)
{
    thread t([this] { forward(true, rxDriver, txDriver); });

    close(rxSockets[0]);
    t.join();
}

BOOST_AUTO_TEST_CASE(it_quits_if_the_right_connection_closes)
{
    thread t([this] { forward(true, rxDriver, txDriver); });

    close(txSockets[1]);
    t.join();
}

BOOST_AUTO_TEST_CASE(it_forwards_data_from_left_to_right)
{
    thread t([this] { forward(true, rxDriver, txDriver); });

    uint8_t buffer[10] = { 1, 2, 3, 4, 5, 6 };
    write(buffer, 10);
    read(buffer, 10);
    close(rxSockets[0]);
    close(txSockets[1]);
    t.join();
}

BOOST_AUTO_TEST_CASE(it_forwards_data_from_right_to_left)
{
    thread t([this] { forward(true, txDriver, rxDriver); });

    uint8_t buffer[10] = { 1, 2, 3, 4, 5, 6 };
    write(buffer, 10);
    read(buffer, 10);
    close(rxSockets[0]);
    close(txSockets[1]);
    t.join();
}

BOOST_AUTO_TEST_SUITE_END()


class PacketForwardDriver : public Driver
{
public:
    PacketForwardDriver() : Driver(100) {}
    int extractPacket(uint8_t const* buffer, size_t buffer_size) const
    {
        for (size_t i = 0; i < buffer_size; ++i) {
            if (buffer[i] == 0) {
                return i + 1;
            }
        }
        return 0;
    }
};

BOOST_FIXTURE_TEST_SUITE(ForwardSuite_PacketMode, ForwardFixture<PacketForwardDriver>)

BOOST_AUTO_TEST_CASE(it_quits_if_the_left_connection_closes)
{
    thread t([this] { forward(false, rxDriver, txDriver); });

    close(rxSockets[0]);
    t.join();
}

BOOST_AUTO_TEST_CASE(it_quits_if_the_right_connection_closes)
{
    thread t([this] { forward(false, rxDriver, txDriver); });

    close(txSockets[1]);
    t.join();
}

BOOST_AUTO_TEST_CASE(it_forwards_whole_packets_from_left_to_right)
{
    thread t([this] { forward(false, rxDriver, txDriver); });

    uint8_t buffer[10] = { 1, 2, 3, 0 };
    write(buffer, 4);
    uint8_t readBuffer[10];
    BOOST_REQUIRE_EQUAL(4, read(readBuffer, 4));
    BOOST_REQUIRE_EQUAL(1, readBuffer[0]);
    BOOST_REQUIRE_EQUAL(2, readBuffer[1]);
    BOOST_REQUIRE_EQUAL(3, readBuffer[2]);
    BOOST_REQUIRE_EQUAL(0, readBuffer[3]);

    close(rxSockets[0]);
    t.join();
}

BOOST_AUTO_TEST_CASE(it_does_not_forward_partial_packets_from_left_to_right)
{
    thread t([this] { forward(false, rxDriver, txDriver); });

    uint8_t buffer[10] = { 1 };
    write(buffer, 1);

    FDStream endpoint(txSockets[1], false);
    BOOST_REQUIRE_EQUAL(0, endpoint.read(buffer, 1));

    close(rxSockets[0]);
    t.join();
}

BOOST_AUTO_TEST_CASE(it_forwards_whole_packets_from_right_to_left)
{
    thread t([this] { forward(false, txDriver, rxDriver); });

    uint8_t buffer[10] = { 1, 2, 3, 0 };
    write(buffer, 4);
    BOOST_REQUIRE_EQUAL(4, read(buffer, 4));
    BOOST_REQUIRE_EQUAL(1, buffer[0]);
    BOOST_REQUIRE_EQUAL(2, buffer[1]);
    BOOST_REQUIRE_EQUAL(3, buffer[2]);
    BOOST_REQUIRE_EQUAL(0, buffer[3]);

    close(rxSockets[0]);
    t.join();
}

BOOST_AUTO_TEST_CASE(it_does_not_forward_partial_packets_from_right_to_left)
{
    thread t([this] { forward(false, txDriver, rxDriver); });

    uint8_t buffer[10] = { 1 };
    write(buffer, 1);

    FDStream endpoint(txSockets[1], false);
    BOOST_REQUIRE_EQUAL(0, endpoint.read(buffer, 1));

    close(rxSockets[0]);
    t.join();
}

BOOST_AUTO_TEST_SUITE_END()
