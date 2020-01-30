#include <boost/test/unit_test.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <iodrivers_base/Driver.hpp>
#include <iostream>
#include <thread>

using namespace std;
using base::Time;
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

int setupDriver(Driver& driver)
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
    if (write(tx, data, size) != size) {
        throw std::runtime_error("failed writing the test data");
    }
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

BOOST_AUTO_TEST_CASE(eof_returns_false_on_valid_file_descriptor) {
    DriverTest test;
    setupDriver(test);
    BOOST_REQUIRE(!test.eof());
}

BOOST_AUTO_TEST_CASE(eof_returns_true_on_a_closed_file_descriptor_after_a_read) {
    DriverTest test;
    int tx = setupDriver(test);
    close(tx);
    BOOST_REQUIRE(!test.eof());

    uint8_t buffer[100];
    BOOST_REQUIRE_THROW(test.readPacket(buffer, 100, base::Time()), TimeoutError);
    BOOST_REQUIRE(test.eof());
}

BOOST_AUTO_TEST_CASE(eof_throws_if_the_driver_does_not_have_a_valid_stream) {
    DriverTest test;
    BOOST_REQUIRE_THROW(test.eof(), std::runtime_error);
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

BOOST_AUTO_TEST_CASE(test_readPacket_times_out_after_packet_timeout_when_there_is_no_data)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);

    uint8_t buffer[256];

    Time tic = Time::now();
    BOOST_REQUIRE_THROW(
        test.readPacket(buffer, 256, base::Time::fromSeconds(0.1)),
        iodrivers_base::TimeoutError
    );
    BOOST_REQUIRE_GT((Time::now() - tic).toSeconds(), 0.08);
}

BOOST_AUTO_TEST_CASE(test_readPacket_times_out_after_packet_timeout_even_if_there_is_a_partial_packet)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);

    uint8_t buffer[256];
    buffer[0] = 0;
    writeToDriver(test, tx, buffer, 1);

    Time tic = Time::now();
    BOOST_REQUIRE_THROW(
        test.readPacket(buffer, 256, base::Time::fromSeconds(0.1)),
        iodrivers_base::TimeoutError
    );
    BOOST_REQUIRE_GT((Time::now() - tic).toSeconds(), 0.08);
}

BOOST_AUTO_TEST_CASE(test_readPacket_times_out_after_a_first_byte_timeout_if_there_is_no_data)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);

    uint8_t buffer[256];

    Time tic = Time::now();
    BOOST_REQUIRE_THROW(
        test.readPacket(buffer, 256, Time::fromSeconds(1), Time::fromSeconds(0.1)),
        iodrivers_base::TimeoutError
    );
    double delay_s = (Time::now() - tic).toSeconds();
    BOOST_REQUIRE_GT(delay_s, 0.08);
    BOOST_REQUIRE_LT(delay_s, 0.2);
}

BOOST_AUTO_TEST_CASE(test_readPacket_times_out_after_the_packet_timeout_regardless_of_the_first_byte_timeout_if_there_is_data)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);

    uint8_t buffer[256];
    buffer[0] = 0;
    writeToDriver(test, tx, buffer, 1);

    Time tic = Time::now();
    BOOST_REQUIRE_THROW(
        test.readPacket(buffer, 256, Time::fromSeconds(0.1), Time()),
        iodrivers_base::TimeoutError
    );
    BOOST_REQUIRE_GT((Time::now() - tic).toSeconds(), 0.08);
}

BOOST_AUTO_TEST_CASE(test_readPacket_reconstructs_packets_of_bytes_arriving_little_by_little_if_it_is_faster_than_packet_timeout)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);

    uint8_t buffer[100];
    auto start = base::Time::now();
    thread writeThread([this, &test, tx]{
        uint8_t packet[4] = { 0, 5, 2, 0 };
        for (uint8_t i = 0; i < 4; ++i) {
            writeToDriver(test, tx, packet + i, 1);
            usleep(10000);
        }
    });
    int c = test.readPacket(
        buffer, 100, Time::fromSeconds(0.2)
    );
    writeThread.join();
    BOOST_REQUIRE_EQUAL(4, c);
    BOOST_REQUIRE_LE(Time::now() - start, Time::fromMilliseconds(100));
}

BOOST_AUTO_TEST_CASE(test_readPacket_throws_a_timeout_if_a_packet_is_not_completed_by_the_packet_timeout)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);

    uint8_t buffer[100];
    auto start = base::Time::now();
    thread writeThread([this, &test, tx]{
        uint8_t packet[4] = { 0, 5, 2, 0 };
        for (uint8_t i = 0; i < 4; ++i) {
            writeToDriver(test, tx, packet + i, 1);
            usleep(20000);
        }
    });
    BOOST_REQUIRE_THROW(
        test.readPacket(buffer, 100, Time::fromSeconds(0.05)),
        iodrivers_base::TimeoutError
    );
    writeThread.join();
    BOOST_REQUIRE_GT(Time::now() - start, Time::fromMilliseconds(45));
}

BOOST_AUTO_TEST_CASE(test_readRaw_throws_if_the_driver_is_not_valid)
{
    DriverTest test;
    BOOST_REQUIRE_THROW(test.readRaw(nullptr, 0), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(test_readRaw_reads_the_bytes_available)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);

    uint8_t msg[16] = { 'g', 'a', 'r', 'b', 0, 'a', 'b', 0, 'b', 'a', 'g', 'e', 0, 'c', 'd', 0 };
    writeToDriver(test, tx, msg, 16);
    uint8_t buffer[16];
    int size = test.readRaw(buffer, 16);
    BOOST_REQUIRE_EQUAL(16, size);
    BOOST_REQUIRE_EQUAL(memcmp(buffer, msg, 16), 0);
}

BOOST_AUTO_TEST_CASE(test_readRaw_consumes_the_bytes_it_has_read)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);

    uint8_t msg[16] = { 'g', 'a', 'r', 'b', 0, 'a', 'b', 0, 'b', 'a', 'g', 'e', 0, 'c', 'd', 0 };
    writeToDriver(test, tx, msg, 16);
    uint8_t expectedBuffer[16] = { 1, 2, 3, 4 };
    uint8_t buffer[16];
    test.readRaw(buffer, 16);

    memcpy(buffer, expectedBuffer, 16);
    int size = test.readRaw(buffer, 4);
    BOOST_REQUIRE_EQUAL(0, size);
    // The buffer should not be modified
    BOOST_REQUIRE_EQUAL(memcmp(buffer, expectedBuffer, 4), 0);
}

BOOST_AUTO_TEST_CASE(test_readRaw_read_bytes_from_the_internal_buffer)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);

    uint8_t msg[16] = { 0, 'g', 'a', 'r' };
    writeToDriver(test, tx, msg, 3);
    uint8_t buffer[100];
    BOOST_REQUIRE_THROW(test.readPacket(buffer, 100), TimeoutError);
    int size = test.readRaw(buffer, 3);
    BOOST_REQUIRE_EQUAL(3, size);
    BOOST_REQUIRE_EQUAL(memcmp(buffer, msg, 3), 0);
}

BOOST_AUTO_TEST_CASE(test_readRaw_consumes_bytes_from_the_internal_buffer_it_has_read)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);

    uint8_t msg[16] = { 0, 'g', 'a', 'r' };
    writeToDriver(test, tx, msg, 3);

    uint8_t buffer[100];
    BOOST_REQUIRE_THROW(test.readPacket(buffer, 100), TimeoutError);
    test.readRaw(buffer, 3);
    int size = test.readRaw(buffer, 3);
    BOOST_REQUIRE_EQUAL(0, size);
}

BOOST_AUTO_TEST_CASE(test_readRaw_concatenates_bytes_from_io_and_internal_buffer)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);

    uint8_t msg0[3] = { 0, 'g', 'a' };
    uint8_t msg1[13] = { 0, 'a', 'b', 'c', 0, 'b', 'a', 'g', 'e', 0, 'c', 'd', 0 };
    writeToDriver(test, tx, msg0, 3);

    uint8_t buffer[100];
    BOOST_REQUIRE_THROW(test.readPacket(msg0, 100), TimeoutError);
    writeToDriver(test, tx, msg1, 13);
    int size = test.readRaw(buffer, 100);
    BOOST_REQUIRE_EQUAL(16, size);
    BOOST_REQUIRE_EQUAL(memcmp(msg0, buffer, 3), 0);
    BOOST_REQUIRE_EQUAL(memcmp(msg1, buffer + 3, 13), 0);
}

BOOST_AUTO_TEST_CASE(test_readRaw_terminates_at_first_byte_timeout_if_there_are_no_chars_coming)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);

    uint8_t buffer[100];
    auto start = base::Time::now();
    int c = test.readRaw(
        buffer, 100, Time::fromMilliseconds(50), Time::fromMilliseconds(10)
    );
    BOOST_REQUIRE_EQUAL(0, c);
    BOOST_REQUIRE_LE(Time::now() - start, Time::fromMilliseconds(30));
}

BOOST_AUTO_TEST_CASE(test_readRaw_terminates_at_packet_timeout_if_a_first_byte_was_received)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);

    uint8_t buffer[100];
    auto start = base::Time::now();
    writeToDriver(test, tx, buffer, 3);
    int c = test.readRaw(
        buffer, 100, Time::fromMilliseconds(50), Time::fromMilliseconds(10)
    );
    BOOST_REQUIRE_EQUAL(3, c);
    BOOST_REQUIRE_GE(Time::now() - start, Time::fromMilliseconds(45));
}

BOOST_AUTO_TEST_CASE(test_readRaw_terminates_at_inter_byte_timeout_regardless_of_packet_timeout)
{
    DriverTest test;
    int tx = setupDriver(test);
    FileGuard tx_guard(tx);

    uint8_t buffer[100];
    auto start = base::Time::now();
    thread writeThread([this,&test,tx]{
        for (uint8_t i = 0; i < 10; ++i) {
            writeToDriver(test, tx, &i, 1);
            usleep(1000);
        }
    });
    int c = test.readRaw(
        buffer, 100, Time::fromSeconds(1), Time::fromSeconds(1),
        Time::fromMilliseconds(10)
    );
    writeThread.join();
    BOOST_REQUIRE_EQUAL(10, c);
    BOOST_REQUIRE_LE(Time::now() - start, Time::fromMilliseconds(100));
}

BOOST_AUTO_TEST_CASE(test_returns_a_parsed_serial_configuration_object)
{
    DriverTest test;

    SerialConfiguration config = test.parseSerialConfiguration("8N1");
    BOOST_REQUIRE_EQUAL(BITS_8, config.byte_size);
    BOOST_REQUIRE_EQUAL(PARITY_NONE, config.parity);
    BOOST_REQUIRE_EQUAL(STOP_BITS_ONE, config.stop_bits);

    config = test.parseSerialConfiguration("5e2");
    BOOST_REQUIRE_EQUAL(BITS_5, config.byte_size);
    BOOST_REQUIRE_EQUAL(PARITY_EVEN, config.parity);
    BOOST_REQUIRE_EQUAL(STOP_BITS_TWO, config.stop_bits);

    config = test.parseSerialConfiguration("7o1");
    BOOST_REQUIRE_EQUAL(BITS_7, config.byte_size);
    BOOST_REQUIRE_EQUAL(PARITY_ODD, config.parity);
    BOOST_REQUIRE_EQUAL(STOP_BITS_ONE, config.stop_bits);
}

BOOST_AUTO_TEST_CASE(test_throws_invalid_argument_if_description_is_invalid)
{
    DriverTest test;
    BOOST_REQUIRE_THROW(test.parseSerialConfiguration("9N1"), invalid_argument);
    BOOST_REQUIRE_THROW(test.parseSerialConfiguration("4N1"), invalid_argument);
    BOOST_REQUIRE_THROW(test.parseSerialConfiguration("8V1"), invalid_argument);
    BOOST_REQUIRE_THROW(test.parseSerialConfiguration("8N3"), invalid_argument);
}

BOOST_AUTO_TEST_SUITE_END()
