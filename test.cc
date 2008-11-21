#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN
#define BOOST_TEST_MODULE "iodrivers"
#define BOOST_AUTO_TEST_MAIN
#include <boost/test/auto_unit_test.hpp>
#include <boost/test/unit_test.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "iodrivers_base.hh"
using namespace std;

class IODriverTest : public IODriver
{
public:
    IODriverTest()
        : IODriver(4) {}

    int extractPacket(uint8_t const* buffer, size_t buffer_size) const
    {
        if (buffer[0] != 0)
            return -1;
        else if (buffer_size < 4)
            return 0;
        else if (buffer[3] == 0)
            return 4;
        else
            return -4;
    }
};

int setupDriver(IODriver& driver)
{
    int pipes[2];
    pipe(pipes);
    int rx = pipes[0];
    int tx = pipes[1];
    fcntl(rx, F_SETFL, O_NONBLOCK);

    driver.setFileDescriptor(rx, true);
    return tx;
}

BOOST_AUTO_TEST_CASE(test_file_guard)
{
    int tx = open("/dev/zero", O_RDONLY);
    BOOST_REQUIRE( tx != -1 );

    { file_guard guard(tx); }
    BOOST_REQUIRE_EQUAL(-1, close(tx));
    BOOST_REQUIRE_EQUAL(EBADF, errno);
}

BOOST_AUTO_TEST_CASE(test_rx_timeout)
{
    IODriverTest test;
    int tx = setupDriver(test);
    file_guard tx_guard(tx);

    uint8_t buffer[100];
    BOOST_REQUIRE_THROW(test.readPacket(buffer, 100, 10), timeout_error);

    write(tx, "a", 1);
    BOOST_REQUIRE_THROW(test.readPacket(buffer, 100, 10), timeout_error);
}

BOOST_AUTO_TEST_CASE(test_rx_first_packet_extraction)
{
    IODriverTest test;
    int tx = setupDriver(test);
    file_guard tx_guard(tx);

    uint8_t buffer[100];
    uint8_t msg[4] = { 0, 'a', 'b', 0 };
    write(tx, msg, 4);
    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE( !memcmp(msg, buffer, 4) );
}

BOOST_AUTO_TEST_CASE(test_rx_partial_packets)
{
    IODriverTest test;
    int tx = setupDriver(test);
    file_guard tx_guard(tx);

    uint8_t buffer[100];
    uint8_t msg[4] = { 0, 'a', 'b', 0 };
    write(tx, msg, 2);
    BOOST_REQUIRE_THROW(test.readPacket(buffer, 100, 10), timeout_error);
    write(tx, msg + 2, 2);
    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE( !memcmp(msg, buffer, 4) );

    write(tx, msg, 4);
    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE( !memcmp(msg, buffer, 4) );
}

BOOST_AUTO_TEST_CASE(test_rx_garbage_removal)
{
    IODriverTest test;
    int tx = setupDriver(test);
    file_guard tx_guard(tx);

    uint8_t buffer[100];
    uint8_t msg[16] = { 'g', 'a', 'r', 'b', 0, 'a', 'b', 0, 'b', 'a', 'g', 'e', 0, 'c', 'd', 0 };
    write(tx, msg, 3);
    BOOST_REQUIRE_THROW(test.readPacket(buffer, 100, 10), timeout_error);
    write(tx, msg + 3, 3);
    BOOST_REQUIRE_THROW(test.readPacket(buffer, 100, 10), timeout_error);
    write(tx, msg + 6, 3);
    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE( !memcmp(msg + 4, buffer, 4) );

    write(tx, msg + 9, 7);
    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE( !memcmp(msg + 12, buffer, 4) );
}

BOOST_AUTO_TEST_CASE(test_rx_packet_extraction_mode)
{
    IODriverTest test;
    int tx = setupDriver(test);
    file_guard tx_guard(tx);

    uint8_t buffer[100];
    uint8_t msg[16] = { 'g', 'a', 'r', 'b', 0, 'a', 'b', 0, 'b', 'a', 'g', 'e', 0, 'c', 'd', 0 };
    write(tx, msg, 16);
    test.setExtractLastPacket(false);

    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE( !memcmp(msg + 4, buffer, 4) );
    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE( !memcmp(msg + 12, buffer, 4) );

    write(tx, msg, 16);
    test.setExtractLastPacket(true);

    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE( !memcmp(msg + 12, buffer, 4) );

    write(tx, msg, 16);
    test.setExtractLastPacket(false);
    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE( !memcmp(msg + 4, buffer, 4) );
    write(tx, msg, 14);
    // We have now one packet from the first write and one packet from the 2nd
    // write. We should get the packet from the second write
    test.setExtractLastPacket(true);
    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE( !memcmp(msg + 4, buffer, 4) );
    // The garbage that was at the end of the second write should have been
    // removed as well
    BOOST_REQUIRE_EQUAL(-1, read(test.getFileDescriptor(), buffer, 1));
    BOOST_REQUIRE_EQUAL(EAGAIN, errno);
    write(tx, msg + 14, 2);
    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE( !memcmp(msg + 12, buffer, 4) );
}

