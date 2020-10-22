#include <boost/test/unit_test.hpp>

#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>

#include <iodrivers_base/Driver.hpp>
#include <iodrivers_base/IOStream.hpp>

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

struct UDPFixture {
    DriverTest test;
    DriverTest server;
    uint8_t sendBuffer[100];
    uint8_t receiveBuffer[100];

    UDPFixture() {
        test.setReadTimeout(base::Time::fromMilliseconds(100));
        sendBuffer[0] = sendBuffer[3] = 0;
    }

    void write() {
        sendBuffer[1] = rand();
        sendBuffer[2] = rand();
        test.writePacket(sendBuffer, 100);
    }

    void read() {
        test.readPacket(receiveBuffer, 100);
    }

    void serverWrite() {
        sendBuffer[1] = rand();
        sendBuffer[2] = rand();
        server.writePacket(sendBuffer, 100);
    }

    void serverRead() {
        server.readPacket(receiveBuffer, 100);
    }

    void validateReceiveBuffer() {
        BOOST_TEST(memcmp(sendBuffer, receiveBuffer, 4) == 0);
    }
};

class UDPServerStreamMock : iodrivers_base::UDPServerStream {
    std::pair<ssize_t, int> mRecvfromReturn;
    std::pair<ssize_t, int> mSendtoReturn;

public:
    int recvfromCalls = 0;
    int sendtoCalls = 0;

    UDPServerStreamMock(UDPServerStream& copyfrom)
        : UDPServerStream(copyfrom) {
    }

    void setRecvfromReturn(ssize_t ret, int err) {
        mRecvfromReturn = make_pair(ret, err);
    }
    std::pair<ssize_t, int> recvfrom(
        uint8_t* buffer, size_t buffer_size, sockaddr* s_other, socklen_t* s_len
    ) {
        recvfromCalls++;

        // Do call recvfrom to clear whatever woke us up
        uint8_t temp_buffer[256];
        if (buffer_size > 256) {
            throw std::invalid_argument(
                "the mocked recvfrom expects buffers to be smaller than 256 bytes"
            );
        }

        (void)::recvfrom(m_fd, temp_buffer, buffer_size, 0, NULL, NULL);
        return mRecvfromReturn;
    }

    void setSendtoReturn(ssize_t ret, int err) {
        mSendtoReturn = make_pair(ret, err);
    }
    std::pair<ssize_t, int> sendto(
        uint8_t const* buffer, size_t buffer_size
    ) {
        sendtoCalls++;
        return mSendtoReturn;
    }

    static UDPServerStreamMock& setup(Driver& driver) {
        auto& udpStream = dynamic_cast<UDPServerStream&>(*driver.getMainStream());
        auto mock = new UDPServerStreamMock(udpStream);

        udpStream.setAutoClose(false);
        driver.setMainStream(mock);
        return *mock;
    }
};

BOOST_FIXTURE_TEST_SUITE(general_udp_behavior, UDPFixture)
    BOOST_AUTO_TEST_CASE(it_reports_hostunreach_by_default_on_read)
    {
        test.openURI("udp://127.0.0.1:1111");
        auto& stream = UDPServerStreamMock::setup(test);
        stream.setRecvfromReturn(-1, EHOSTUNREACH);

        BOOST_REQUIRE_EXCEPTION(
            test.readPacket(receiveBuffer, 100), UnixError,
            [](UnixError const& e) -> bool { return e.error == EHOSTUNREACH; }
        );
    }

    BOOST_AUTO_TEST_CASE(it_ignores_hostunreach_by_default_on_read_if_configured)
    {
        test.openURI("udp://127.0.0.1:1111?ignore_hostunreach=1");
        auto& stream = UDPServerStreamMock::setup(test);
        stream.setRecvfromReturn(-1, EHOSTUNREACH);
        BOOST_REQUIRE_THROW(
            test.readPacket(receiveBuffer, 100), TimeoutError
        );
    }

    BOOST_AUTO_TEST_CASE(it_reports_netunreach_by_default_on_read)
    {
        test.openURI("udp://127.0.0.1:1111");
        auto& stream = UDPServerStreamMock::setup(test);
        stream.setRecvfromReturn(-1, ENETUNREACH);

        BOOST_REQUIRE_EXCEPTION(
            test.readPacket(receiveBuffer, 100), UnixError,
            [](UnixError const& e) -> bool { return e.error == ENETUNREACH; }
        );
    }

    BOOST_AUTO_TEST_CASE(it_ignores_netunreach_by_default_on_read_if_configured)
    {
        test.openURI("udp://127.0.0.1:1111?ignore_netunreach=1");
        auto& stream = UDPServerStreamMock::setup(test);
        stream.setRecvfromReturn(-1, ENETUNREACH);
        BOOST_REQUIRE_THROW(
            test.readPacket(receiveBuffer, 100), TimeoutError
        );
    }

    BOOST_AUTO_TEST_CASE(it_reports_hostunreach_by_default_on_write)
    {
        test.openURI("udp://127.0.0.1:1111");
        auto& stream = UDPServerStreamMock::setup(test);
        stream.setSendtoReturn(-1, EHOSTUNREACH);

        BOOST_REQUIRE_EXCEPTION(
            test.writePacket(receiveBuffer, 100), UnixError,
            [](UnixError const& e) -> bool { return e.error == EHOSTUNREACH; }
        );
    }

    BOOST_AUTO_TEST_CASE(it_ignores_hostunreach_by_default_on_write_if_configured_and_does_not_timeout)
    {
        test.openURI("udp://127.0.0.1:1111?ignore_hostunreach=1");
        auto& stream = UDPServerStreamMock::setup(test);
        stream.setSendtoReturn(-1, EHOSTUNREACH);
        test.writePacket(receiveBuffer, 100);
    }

    BOOST_AUTO_TEST_CASE(it_reports_netunreach_by_default_on_write)
    {
        test.openURI("udp://127.0.0.1:1111");
        auto& stream = UDPServerStreamMock::setup(test);
        stream.setSendtoReturn(-1, ENETUNREACH);

        BOOST_REQUIRE_EXCEPTION(
            test.writePacket(receiveBuffer, 100), UnixError,
            [](UnixError const& e) -> bool { return e.error == ENETUNREACH; }
        );
    }

    BOOST_AUTO_TEST_CASE(it_ignores_netunreach_by_default_on_write_if_configured_and_does_not_timeout)
    {
        test.openURI("udp://127.0.0.1:1111?ignore_netunreach=1");
        auto& stream = UDPServerStreamMock::setup(test);
        stream.setSendtoReturn(-1, ENETUNREACH);
        test.writePacket(receiveBuffer, 100);
    }

    BOOST_AUTO_TEST_CASE(it_does_not_timeout_for_CONNREFUSED_if_ignored)
    {
        test.openURI("udp://127.0.0.1:1111?ignore_connrefused=1");
        auto& stream = UDPServerStreamMock::setup(test);
        stream.setSendtoReturn(-1, ECONNREFUSED);
        test.writePacket(receiveBuffer, 100);
    }

    BOOST_AUTO_TEST_CASE(it_does_not_report_incoming_bytes_for_a_connrefused_if_it_is_ignored)
    {
        test.openURI("udp://127.0.0.1:1111?ignore_connrefused=1");
        test.writePacket(sendBuffer, 100);
        BOOST_REQUIRE_THROW(
            test.getMainStream()->waitRead(base::Time::fromMilliseconds(100)),
            TimeoutError
        );
    }

    BOOST_AUTO_TEST_CASE(it_reports_in_read_a_connrefused_that_was_discovered_in_waitRead)
    {
        test.openURI("udp://127.0.0.1:1111?ignore_connrefused=0");
        test.writePacket(sendBuffer, 100);
        test.getMainStream()->waitRead(base::Time::fromMilliseconds(100));
        BOOST_REQUIRE_EXCEPTION(
            test.readPacket(receiveBuffer, 100), UnixError,
            [](UnixError const& e) -> bool { return e.error == ECONNREFUSED; }
        );
    }

    BOOST_AUTO_TEST_CASE(it_does_not_report_incoming_bytes_for_a_hostunreach_if_it_is_ignored)
    {
        test.openURI("udp://127.0.0.1:1111?local_port=1112&ignore_hostunreach=1");
        // Need to send something or the real FDStream::waitRead will timeout
        server.openURI("udp://127.0.0.1:1112?local_port=1111");
        serverWrite();

        auto& stream = UDPServerStreamMock::setup(test);
        stream.setRecvfromReturn(-1, EHOSTUNREACH);

        BOOST_REQUIRE_THROW(
            test.getMainStream()->waitRead(base::Time::fromMilliseconds(100)),
            TimeoutError
        );
        BOOST_REQUIRE(stream.recvfromCalls > 0);
    }

    BOOST_AUTO_TEST_CASE(it_reports_in_read_a_hostunreach_that_was_discovered_in_waitRead)
    {
        test.openURI("udp://127.0.0.1:1111?local_port=1112&ignore_hostunreach=0");
        // Need to send something or the real FDStream::waitRead will timeout
        server.openURI("udp://127.0.0.1:1112?local_port=1111");
        serverWrite();

        auto& stream = UDPServerStreamMock::setup(test);
        stream.setRecvfromReturn(-1, EHOSTUNREACH);

        test.getMainStream()->waitRead(base::Time::fromMilliseconds(100));
        BOOST_REQUIRE_EXCEPTION(
            test.readPacket(receiveBuffer, 100), UnixError,
            [](UnixError const& e) -> bool { return e.error == EHOSTUNREACH; }
        );
        BOOST_REQUIRE(stream.recvfromCalls > 0);
    }

    BOOST_AUTO_TEST_CASE(it_does_not_report_incoming_bytes_for_a_netunreach_if_it_is_ignored)
    {
        test.openURI("udp://127.0.0.1:1111?ignore_connrefused=0&ignore_netunreach=1");
        // Cheat. Do generate an error, but then override it later with the mock
        // This makes sure the FDStream::waitRead returns. Otherwise, it is the one
        // generating the error
        test.writePacket(sendBuffer, 100);

        auto& stream = UDPServerStreamMock::setup(test);
        stream.setRecvfromReturn(-1, ENETUNREACH);

        BOOST_REQUIRE_THROW(
            test.getMainStream()->waitRead(base::Time::fromMilliseconds(100)),
            TimeoutError
        );
        BOOST_REQUIRE(stream.recvfromCalls > 0);
    }

    BOOST_AUTO_TEST_CASE(it_reports_in_read_a_netunreach_that_was_discovered_in_waitRead)
    {
        test.openURI("udp://127.0.0.1:1111?local_port=1112&ignore_netunreach=0");
        // Need to send something or the real FDStream::waitRead will timeout
        server.openURI("udp://127.0.0.1:1112?local_port=1111");
        serverWrite();

        auto& stream = UDPServerStreamMock::setup(test);
        stream.setRecvfromReturn(-1, ENETUNREACH);

        test.getMainStream()->waitRead(base::Time::fromMilliseconds(100));
        BOOST_REQUIRE_EXCEPTION(
            test.readPacket(receiveBuffer, 100), UnixError,
            [](UnixError const& e) -> bool { return e.error == ENETUNREACH; }
        );
        BOOST_REQUIRE(stream.recvfromCalls > 0);
    }

    BOOST_AUTO_TEST_CASE(it_reports_an_error_from_waitRead_only_once)
    {
        test.openURI("udp://127.0.0.1:1111?ignore_connrefused=0");
        test.writePacket(sendBuffer, 100);
        test.getMainStream()->waitRead(base::Time::fromMilliseconds(100));
        BOOST_REQUIRE_THROW(test.readPacket(receiveBuffer, 100), UnixError);
        BOOST_REQUIRE_THROW(test.readPacket(receiveBuffer, 100), TimeoutError);
    }

    BOOST_AUTO_TEST_CASE(it_returns_right_away_in_waitRead_if_there_is_a_pending_error)
    {
        test.openURI("udp://127.0.0.1:1111?ignore_connrefused=0");
        test.writePacket(sendBuffer, 100);
        test.getMainStream()->waitRead(base::Time::fromMilliseconds(100));
        test.getMainStream()->waitRead(base::Time::fromMilliseconds(100));
        BOOST_REQUIRE_THROW(test.readPacket(receiveBuffer, 100), UnixError);
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(udp_without_local_port, UDPFixture)

    BOOST_AUTO_TEST_CASE(it_reports_connrefused_by_default_on_read)
    {
        test.openURI("udp://127.0.0.1:1111");
        test.writePacket(sendBuffer, 100);
        BOOST_REQUIRE_EXCEPTION(
            test.readPacket(receiveBuffer, 100), UnixError,
            [](UnixError const& e) -> bool { return e.error == ECONNREFUSED; }
        );
    }

    BOOST_AUTO_TEST_CASE(it_reports_connrefused_by_default_on_write)
    {
        test.openURI("udp://127.0.0.1:1111");
        test.writePacket(sendBuffer, 100);
        BOOST_REQUIRE_EXCEPTION(
            test.writePacket(receiveBuffer, 100), UnixError,
            [](UnixError const& e) -> bool { return e.error == ECONNREFUSED; }
        );
    }

    BOOST_AUTO_TEST_CASE(it_ignores_connrefused_on_read_if_configured)
    {
        test.openURI("udp://127.0.0.1:1111?ignore_connrefused=1");
        test.writePacket(sendBuffer, 100);
        BOOST_REQUIRE_THROW(test.readPacket(receiveBuffer, 100),
                            iodrivers_base::TimeoutError);
    }

    BOOST_AUTO_TEST_CASE(it_ignores_connrefused_on_write_if_configured)
    {
        test.openURI("udp://127.0.0.1:1111?ignore_connrefused=1");
        test.writePacket(sendBuffer, 100);
        test.writePacket(sendBuffer, 100);
    }

    BOOST_AUTO_TEST_CASE(it_sends_to_the_configured_remote)
    {
        server.openURI("udpserver://1111");
        test.openURI("udp://127.0.0.1:1111");
        write();
        serverRead();
        validateReceiveBuffer();
    }

    BOOST_AUTO_TEST_CASE(it_receives_packets_from_the_configured_remote)
    {
        server.openURI("udpserver://1111");
        test.openURI("udp://127.0.0.1:1111");
        write();
        serverRead();
        serverWrite();
        read();
        validateReceiveBuffer();
    }

    int inetLocalPort(int fd) {
        // Find the socket's local port
        sockaddr_storage addr;
        socklen_t size = sizeof(addr);
        getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &size);

        if (addr.ss_family != AF_INET) {
            throw std::invalid_argument("not an IPv4 address");
        }
        return ntohs(reinterpret_cast<sockaddr_in&>(addr).sin_port);
    }

    BOOST_AUTO_TEST_CASE(it_rejects_packets_from_a_different_remote_by_default)
    {
        test.openURI("udp://127.0.0.1:1111");

        // Make sure the remote UDP stream below does not get port 1111
        server.openURI("udpserver://1111");
        int port = inetLocalPort(test.getFileDescriptor());
        DriverTest remote;
        remote.openURI("udp://127.0.0.1:" + to_string(port));
        remote.writePacket(sendBuffer, 100);
        BOOST_REQUIRE_THROW(read(), iodrivers_base::TimeoutError);
    }

    BOOST_AUTO_TEST_CASE(it_accepts_packets_from_any_host_when_created_unconnected)
    {
        test.openURI("udp://127.0.0.1:1111?connected=0&ignore_connrefused=1");

        // Make sure the remote UDP stream below does not get port 1111
        server.openURI("udpserver://1111");
        int port = inetLocalPort(test.getFileDescriptor());
        DriverTest remote;
        remote.openURI("udp://127.0.0.1:" + to_string(port));
        remote.writePacket(sendBuffer, 100);
        read();
        validateReceiveBuffer();
    }

    BOOST_AUTO_TEST_CASE(it_throws_when_trying_to_create_an_unconnected_socket_that_should_report_connrefused)
    {
        BOOST_REQUIRE_THROW(
            test.openURI("udp://127.0.0.1:1111?connected=0&ignore_connrefused=0"),
            invalid_argument
        );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(udp_with_local_port, UDPFixture)

    BOOST_AUTO_TEST_CASE(it_reports_connrefused_on_read_if_configured)
    {
        test.openURI("udp://127.0.0.1:1111?local_port=5000&connected=1&ignore_connrefused=0");
        test.writePacket(sendBuffer, 100);
        BOOST_REQUIRE_EXCEPTION(
            test.readPacket(receiveBuffer, 100), UnixError,
            [](UnixError const& e) -> bool { return e.error == ECONNREFUSED; }
        );
    }

    BOOST_AUTO_TEST_CASE(it_reports_connrefused_on_write_if_configured)
    {
        test.openURI("udp://127.0.0.1:1111?local_port=5000&connected=1&ignore_connrefused=0");
        test.writePacket(sendBuffer, 100);
        BOOST_REQUIRE_EXCEPTION(
            test.writePacket(receiveBuffer, 100), UnixError,
            [](UnixError const& e) -> bool { return e.error == ECONNREFUSED; }
        );
    }

    BOOST_AUTO_TEST_CASE(it_throws_if_ignore_connrefused_is_zero_but_connected_is_not_set)
    {
        BOOST_REQUIRE_THROW(
            test.openURI("udp://127.0.0.1:1111?local_port=5000&ignore_connrefused=0"),
            invalid_argument
        );
    }

    BOOST_AUTO_TEST_CASE(it_ignores_connrefused_on_read_by_default)
    {
        test.openURI("udp://127.0.0.1:1111?local_port=5000");
        test.writePacket(sendBuffer, 100);
        BOOST_REQUIRE_THROW(test.readPacket(receiveBuffer, 100),
                            iodrivers_base::TimeoutError);
    }

    BOOST_AUTO_TEST_CASE(it_ignores_connrefused_on_write_by_default)
    {
        test.openURI("udp://127.0.0.1:1111?local_port=5000");
        test.writePacket(sendBuffer, 100);
        test.writePacket(sendBuffer, 100);
    }

    BOOST_AUTO_TEST_CASE(it_sends_to_the_configured_remote)
    {
        server.openURI("udpserver://1111");
        test.openURI("udp://127.0.0.1:1111?local_port=5000");
        write();
        serverRead();
        validateReceiveBuffer();
    }

    BOOST_AUTO_TEST_CASE(it_receives_packets_from_the_configured_remote)
    {
        server.openURI("udpserver://1111");
        test.openURI("udp://127.0.0.1:1111?local_port=5000");
        write();
        serverRead();
        serverWrite();
        read();
        validateReceiveBuffer();
    }

    BOOST_AUTO_TEST_CASE(it_rejects_packets_from_a_different_remote_if_created_connected)
    {
        test.openURI("udp://127.0.0.1:1111?local_port=5000&connected=1");

        // Make sure the remote UDP stream below does not get port 1111
        server.openURI("udpserver://1111");
        DriverTest remote;
        remote.openURI("udp://127.0.0.1:5000");
        remote.writePacket(sendBuffer, 100);
        BOOST_REQUIRE_THROW(read(), iodrivers_base::TimeoutError);
    }

    BOOST_AUTO_TEST_CASE(it_accepts_packets_from_any_host_by_default)
    {
        test.openURI("udp://127.0.0.1:1111?local_port=5000");

        // Make sure the remote UDP stream below does not get port 1111
        server.openURI("udpserver://1111");
        DriverTest remote;
        remote.openURI("udp://127.0.0.1:5000");
        remote.writePacket(sendBuffer, 100);
        read();
        validateReceiveBuffer();
    }

    BOOST_AUTO_TEST_CASE(it_throws_when_trying_to_create_an_unconnected_socket_that_should_report_connrefused)
    {
        BOOST_REQUIRE_THROW(
            test.openURI("udp://127.0.0.1:1111?local_port=5000&connected=0&ignore_connrefused=0"),
            invalid_argument
        );
    }

BOOST_AUTO_TEST_SUITE_END()

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

BOOST_AUTO_TEST_CASE(test_recv_from_bidirectional_udp_backward)
{
    DriverTest test;
    uint8_t buffer[100];
    int count;
    uint8_t msg[4] = { 0, 'a', 'b', 0 };

    test.openURI("udp://127.0.0.1:3135:2125");

    recv_test();

    count = test.readPacket(buffer, 100, 200);
    BOOST_REQUIRE_EQUAL(count, 4);
    BOOST_REQUIRE_EQUAL(memcmp(buffer, msg, count), 0);

    test.close();
}

BOOST_AUTO_TEST_CASE(test_recv_from_bidirectional_udp)
{
    DriverTest test;
    uint8_t buffer[100];
    int count;
    uint8_t msg[4] = { 0, 'a', 'b', 0 };

    test.openURI("udp://127.0.0.1:3135?local_port=2125");

    recv_test();

    count = test.readPacket(buffer, 100, 200);
    BOOST_REQUIRE_EQUAL(count, 4);
    BOOST_REQUIRE_EQUAL(memcmp(buffer, msg, count), 0);

    test.close();
}

BOOST_AUTO_TEST_CASE(test_send_from_bidirectional_udp_backward)
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

BOOST_AUTO_TEST_CASE(test_send_from_bidirectional_udp)
{
    DriverTest test;
    DriverTest peer;

    uint8_t buffer[100];
    int count = 0;
    uint8_t msg[4] = { 0, 'a', 'b', 0 };

    peer.openURI("udpserver://4145");
    test.openURI("udp://127.0.0.1:4145?local_port=5155");

    test.writePacket(msg, 4);
    count = peer.readPacket(buffer, 100, 500);

    BOOST_TEST(count == 4);
    BOOST_TEST(memcmp(buffer, msg, count) == 0);
}

BOOST_AUTO_TEST_CASE(send_from_bidirectional_udp_ignores_econnrefused)
{
    DriverTest test;
    BOOST_REQUIRE_NO_THROW(test.openURI("udp://127.0.0.1:4145?local_port=5155"));
    uint8_t buffer[100];
    test.writePacket(buffer, 100);
    test.writePacket(buffer, 100);
    test.writePacket(buffer, 100);
    test.writePacket(buffer, 100);
    BOOST_REQUIRE_THROW(test.readPacket(buffer, 100), TimeoutError);
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
