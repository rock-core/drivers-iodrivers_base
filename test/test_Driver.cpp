#include <boost/test/unit_test.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <iodrivers_base/Driver.hpp>
#include <iostream>
using namespace std;
using namespace iodrivers_base;

class DriverTest : public Driver
{
public:
    DriverTest()
        : Driver(100) {}

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

template<typename DriverType>
int setupDriver(DriverType& driver)
{
    int pipes[2];
    BOOST_REQUIRE(pipe(pipes) == 0);
    int rx = pipes[0];
    int tx = pipes[1];

    long fd_flags = fcntl(rx, F_GETFL);
    fcntl(rx, F_SETFL, fd_flags | O_NONBLOCK);

    driver.setFileDescriptor(rx, true);
    return tx;
}

void writeToDriver(Driver& driver, int tx, uint8_t const* data, int size)
{
    int tx_bytes = write(tx, data, size);
    if (tx_bytes != size)
        throw std::runtime_error("failed to write");
}

BOOST_AUTO_TEST_SUITE(FileGuardSuite)

BOOST_AUTO_TEST_CASE(test_FileGuard)
{
    int tx = open("/dev/zero", O_RDONLY);
    BOOST_REQUIRE( tx != -1 );

    { FileGuard guard(tx); }
    BOOST_REQUIRE_EQUAL(-1, close(tx));
    BOOST_REQUIRE_EQUAL(EBADF, errno);
}

BOOST_AUTO_TEST_SUITE_END()




BOOST_AUTO_TEST_SUITE(DriverSuite)

void common_rx_timeout(DriverTest& test, int tx)
{
    uint8_t buffer[100];
    BOOST_REQUIRE_THROW(test.readPacket(buffer, 100, 10), TimeoutError);

    uint8_t data[1] = { 'a' };
    writeToDriver(test, tx, data, 1);
    BOOST_REQUIRE_THROW(test.readPacket(buffer, 100, 10), TimeoutError);
}
BOOST_AUTO_TEST_CASE(test_rx_timeout)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);
    common_rx_timeout(test, tx);
}

BOOST_AUTO_TEST_CASE(test_rx_first_byte_timeout)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);

    uint8_t buffer[100];
    try
    {
        test.readPacket(buffer, 100, 10, 1);
        BOOST_REQUIRE(false);
    }
    catch(TimeoutError const& e)
    {
        BOOST_REQUIRE_EQUAL(TimeoutError::FIRST_BYTE, e.type);
    }

    BOOST_REQUIRE_EQUAL(write(tx, "a", 1), 1);
    try
    {
        test.readPacket(buffer, 100, 10, 1);
        BOOST_REQUIRE(false);
    }
    catch(TimeoutError const& e)
    {
        BOOST_REQUIRE_EQUAL(TimeoutError::PACKET, e.type);
    }

    try
    {
        test.readPacket(buffer, 100, 10, 1);
        BOOST_REQUIRE(false);
    }
    catch(TimeoutError const& e)
    {
        BOOST_REQUIRE_EQUAL(TimeoutError::FIRST_BYTE, e.type);
    }
}

BOOST_AUTO_TEST_CASE(test_open_sets_nonblock)
{
    DriverTest test;

    int pipes[2];
    BOOST_REQUIRE_EQUAL(pipe(pipes), 0);
    int rx = pipes[0];
    int tx = pipes[1];
    test.setFileDescriptor(rx, true);

    FileGuard tx_guard(tx);

    uint8_t buffer[100];
    BOOST_REQUIRE_THROW(test.readPacket(buffer, 100, 10), TimeoutError);

    BOOST_REQUIRE_EQUAL(1, write(tx, "a", 1));
    BOOST_REQUIRE_THROW(test.readPacket(buffer, 100, 10), TimeoutError);
}

void common_rx_first_packet_extraction(Driver& test, int tx)
{
    uint8_t buffer[100];
    uint8_t msg[4] = { 0, 'a', 'b', 0 };
    writeToDriver(test, tx, msg, 4);
    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE_EQUAL(0, test.getStats().tx);
    BOOST_REQUIRE_EQUAL(4, test.getStats().good_rx);
    BOOST_REQUIRE_EQUAL(0, test.getStats().bad_rx);
    BOOST_REQUIRE( !memcmp(msg, buffer, 4) );
}
BOOST_AUTO_TEST_CASE(test_rx_first_packet_extraction)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);
    common_rx_first_packet_extraction(test, tx);
}

void common_rx_partial_packets(Driver& test, int tx)
{
    uint8_t buffer[100];
    uint8_t msg[4] = { 0, 'a', 'b', 0 };
    writeToDriver(test, tx, msg, 2);
    BOOST_REQUIRE_THROW(test.readPacket(buffer, 100, 10), TimeoutError);
    writeToDriver(test, tx, msg + 2, 2);
    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE_EQUAL(0, test.getStats().tx);
    BOOST_REQUIRE_EQUAL(4, test.getStats().good_rx);
    BOOST_REQUIRE_EQUAL(0, test.getStats().bad_rx);
    BOOST_REQUIRE( !memcmp(msg, buffer, 4) );

    writeToDriver(test, tx, msg, 4);
    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE_EQUAL(0, test.getStats().tx);
    BOOST_REQUIRE_EQUAL(8, test.getStats().good_rx);
    BOOST_REQUIRE_EQUAL(0, test.getStats().bad_rx);
    BOOST_REQUIRE( !memcmp(msg, buffer, 4) );
}
BOOST_AUTO_TEST_CASE(test_rx_partial_packets)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);
    common_rx_partial_packets(test, tx);
}

void common_rx_garbage_removal(Driver& test, int tx)
{
    uint8_t buffer[100];
    uint8_t msg[16] = { 'g', 'a', 'r', 'b', 0, 'a', 'b', 0, 'b', 'a', 'g', 'e', 0, 'c', 'd', 0 };
    writeToDriver(test, tx, msg, 3);
    BOOST_REQUIRE_THROW(test.readPacket(buffer, 100, 10), TimeoutError);
    BOOST_REQUIRE_EQUAL(0, test.getStats().tx);
    BOOST_REQUIRE_EQUAL(0, test.getStats().good_rx);
    BOOST_REQUIRE_EQUAL(3, test.getStats().bad_rx);
    writeToDriver(test, tx, msg + 3, 3);
    BOOST_REQUIRE_THROW(test.readPacket(buffer, 100, 10), TimeoutError);
    BOOST_REQUIRE_EQUAL(0, test.getStats().tx);
    BOOST_REQUIRE_EQUAL(0, test.getStats().good_rx);
    BOOST_REQUIRE_EQUAL(4, test.getStats().bad_rx);
    writeToDriver(test, tx, msg + 6, 3);
    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE_EQUAL(0, test.getStats().tx);
    BOOST_REQUIRE_EQUAL(4, test.getStats().good_rx);
    BOOST_REQUIRE_EQUAL(4, test.getStats().bad_rx);
    BOOST_REQUIRE( !memcmp(msg + 4, buffer, 4) );

    writeToDriver(test, tx, msg + 9, 7);
    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE_EQUAL(0, test.getStats().tx);
    BOOST_REQUIRE_EQUAL(8, test.getStats().good_rx);
    BOOST_REQUIRE_EQUAL(8, test.getStats().bad_rx);
    BOOST_REQUIRE( !memcmp(msg + 12, buffer, 4) );
}
BOOST_AUTO_TEST_CASE(test_rx_garbage_removal)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);
    common_rx_garbage_removal(test, tx);
}

void common_rx_packet_extraction_mode(Driver& test, int tx)
{
    uint8_t buffer[100];
    uint8_t msg[16] = { 'g', 'a', 'r', 'b', 0, 'a', 'b', 0, 'b', 'a', 'g', 'e', 0, 'c', 'd', 0 };
    writeToDriver(test, tx, msg, 16);
    test.setExtractLastPacket(false);

    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE_EQUAL(0, test.getStats().tx);
    BOOST_REQUIRE_EQUAL(4, test.getStats().good_rx);
    BOOST_REQUIRE_EQUAL(4, test.getStats().bad_rx);
    BOOST_REQUIRE( !memcmp(msg + 4, buffer, 4) );
    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE_EQUAL(0, test.getStats().tx);
    BOOST_REQUIRE_EQUAL(8, test.getStats().good_rx);
    BOOST_REQUIRE_EQUAL(8, test.getStats().bad_rx);
    BOOST_REQUIRE( !memcmp(msg + 12, buffer, 4) );
    BOOST_REQUIRE_EQUAL(8, test.getStats().good_rx);
    BOOST_REQUIRE_EQUAL(8, test.getStats().bad_rx);

    writeToDriver(test, tx, msg, 16);
    test.setExtractLastPacket(true);

    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE_EQUAL(0, test.getStats().tx);
    // 16 bytes: even though one package has not been returned, it should still
    // be counted
    BOOST_REQUIRE_EQUAL(16, test.getStats().good_rx);
    BOOST_REQUIRE_EQUAL(16, test.getStats().bad_rx);
    BOOST_REQUIRE( !memcmp(msg + 12, buffer, 4) );

    writeToDriver(test, tx, msg, 16);
    test.setExtractLastPacket(false);
    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE_EQUAL(0, test.getStats().tx);
    BOOST_REQUIRE_EQUAL(20, test.getStats().good_rx);
    BOOST_REQUIRE_EQUAL(20, test.getStats().bad_rx);
    BOOST_REQUIRE( !memcmp(msg + 4, buffer, 4) );
    writeToDriver(test, tx, msg, 14);
    // We have now one packet from the first write and one packet from the 2nd
    // write. We should get the packet from the second write
    test.setExtractLastPacket(true);
    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
    BOOST_REQUIRE_EQUAL(0, test.getStats().tx);
    BOOST_REQUIRE_EQUAL(28, test.getStats().good_rx);
    if (test.isValid())
        BOOST_REQUIRE_EQUAL(32, test.getStats().bad_rx);
    else
        BOOST_REQUIRE_EQUAL(36, test.getStats().bad_rx);
    BOOST_REQUIRE( !memcmp(msg + 4, buffer, 4) );

    if (test.isValid())
    {
        // The garbage that was at the end of the second write should have been
        // removed as well
        BOOST_REQUIRE_EQUAL(-1, read(test.getFileDescriptor(), buffer, 1));
        BOOST_REQUIRE_EQUAL(EAGAIN, errno);
        writeToDriver(test, tx, msg + 14, 2);
        BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10));
        BOOST_REQUIRE( !memcmp(msg + 12, buffer, 4) );
    }
}
BOOST_AUTO_TEST_CASE(test_rx_packet_extraction_mode)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);
    common_rx_packet_extraction_mode(test, tx);
}

BOOST_AUTO_TEST_CASE(test_hasPacket_returns_false_on_empty_internal_buffer)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);
    BOOST_REQUIRE(!test.hasPacket());
}

BOOST_AUTO_TEST_CASE(test_hasPacket_returns_false_on_internal_buffer_with_garbage)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);
    writeToDriver(test, tx, reinterpret_cast<uint8_t const*>("12\x0  \x0 3"), 8);
    uint8_t buffer[100];
    BOOST_REQUIRE_EQUAL(4, test.readPacket(buffer, 100, 10, 1));
    BOOST_REQUIRE(!test.hasPacket());
}

BOOST_AUTO_TEST_CASE(test_open_bidirectional_udp)
{
    DriverTest test;

    BOOST_REQUIRE_NO_THROW(test.openURI("udp://127.0.0.1:1111:2222"));
    test.close();
}

void recv_test()
{
    DriverTest test;
    uint8_t msg[4] = { 0, 'a', 'b', 0 };

    test.openURI("udp://127.0.0.1:2125");

    test.writePacket(msg, 4);
    test.close();
}

BOOST_AUTO_TEST_CASE(test_recv_from_bidirectional_udp)
{
    DriverTest test;
    uint8_t buffer[100];
    int count;
    uint8_t msg[4] = { 0, 'a', 'b', 0 };

    BOOST_REQUIRE_NO_THROW(test.openURI("udp://127.0.0.1:3135:2125"));

    recv_test();

    BOOST_REQUIRE_NO_THROW((count = test.readPacket(buffer, 100, 200)));
    BOOST_REQUIRE_EQUAL(count, 4);
    BOOST_REQUIRE_EQUAL(memcmp(buffer, msg, count), 0);

    test.close();
}

BOOST_AUTO_TEST_CASE(test_send_from_bidirectional_udp)
{
    DriverTest test;
    DriverTest peer;

    uint8_t buffer[100];
    int count = 0;
    uint8_t msg[4] = { 0, 'a', 'b', 0 };

    BOOST_REQUIRE_NO_THROW(peer.openURI("udpserver://4145"));
    BOOST_REQUIRE_NO_THROW(test.openURI("udp://127.0.0.1:4145:5155"));

    BOOST_REQUIRE_NO_THROW(test.writePacket(msg, 4));
    BOOST_REQUIRE_NO_THROW(count = peer.readPacket(buffer, 100, 500));

    test.close();
    peer.close();

    BOOST_REQUIRE((count == 4) && (memcmp(buffer, msg, count) == 0));
}

class FlushReadBufferTest : public Driver
{
public:
    FlushReadBufferTest()
        : Driver(100) {}

    int extractPacket(uint8_t const* buffer, size_t buffer_size) const
    {
        if (buffer[0] != 0)
            return -1;
        int size = buffer[1];
        if (buffer_size < size)
            return 0;

        if (buffer[size - 1] == 0)
            return size;
        return -1;
    }
};

BOOST_AUTO_TEST_CASE(test_flushReadBuffer_skips_partial_packages_until_it_finds_one)
{
    FlushReadBufferTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);
    writeToDriver(test, tx, reinterpret_cast<uint8_t const*>("\x0\xf\x0\xf\x0\x3\x0"), 7);
    uint8_t buffer[100];
    size_t size = test.flushReadBuffer(buffer, 100);
    BOOST_REQUIRE_EQUAL(3, size);
    BOOST_REQUIRE(!memcmp(buffer, "\x0\x3\x0", 3));
}

BOOST_AUTO_TEST_CASE(test_flushReadBuffer_completely_empties_the_read_buffer_if_there_is_no_packet_in_it)
{
    FlushReadBufferTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);
    writeToDriver(test, tx, reinterpret_cast<uint8_t const*>("\x0\xf\x0\xf\x0\x3"), 6);
    uint8_t buffer[100];
    BOOST_REQUIRE_EQUAL(0, test.flushReadBuffer(buffer, 100));
    BOOST_REQUIRE(test.isReadBufferEmpty());
}
BOOST_AUTO_TEST_CASE(test_flushReadBuffer_accounts_for_the_lost_bytes_as_bad_rx)
{
    FlushReadBufferTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);
    writeToDriver(test, tx, reinterpret_cast<uint8_t const*>("\x0\xf\x0\xf\x0\x3\x0"), 7);
    uint8_t buffer[100];
    test.flushReadBuffer(buffer, 100);
    BOOST_REQUIRE_EQUAL(4, test.getStats().bad_rx);
}

BOOST_AUTO_TEST_CASE(test_readPacket_will_flush_the_read_buffer_if_setFlushOnTimeout_is_set)
{
    FlushReadBufferTest test;
    test.setFlushOnTimeout(true);
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);
    writeToDriver(test, tx, reinterpret_cast<uint8_t const*>("\x0\xf\x0\xf\x0\x3\x0"), 7);
    uint8_t buffer[100];
    size_t size = test.readPacket(buffer, 100);
    BOOST_REQUIRE_EQUAL(3, size);
    BOOST_REQUIRE(!memcmp(buffer, "\x0\x3\x0", 3));
}

BOOST_AUTO_TEST_CASE(test_readPacket_will_throw_a_timeout_error_if_setFlushOnTimeout_is_set_and_no_package_is_available)
{
    FlushReadBufferTest test;
    test.setFlushOnTimeout(true);
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);
    writeToDriver(test, tx, reinterpret_cast<uint8_t const*>("\x0\xf\x0\xff sfa\x0\x3"), 10);
    uint8_t buffer[100];
    BOOST_REQUIRE_THROW(test.readPacket(buffer, 100), TimeoutError);
}
