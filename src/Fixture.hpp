#ifndef IODRIVERS_BASE_BOOST_FIXTURE_HPP
#define IODRIVERS_BASE_BOOST_FIXTURE_HPP

#include <iodrivers_base/TestStream.hpp>
#include <vector>
#include <iodrivers_base/Exceptions.hpp>

namespace iodrivers_base
{
    /** Test harness for `iodrivers_base::Driver` subclasses
     *
     * Do not include Fixture.hpp directly, either include
     *
     *      #include <iodrivers_base/FixtureBoostTest.hpp>
     *
     * for Boost.Test and
     *
     *      #include <iodrivers_base/FixtureGTest.hpp>
     *
     * for Google Test
     *
     * This harness allows you to interact with the driver class from both the
     * "driver" side and the "device" side.
     *
     * The "driver" side is the driver's caller side. It is the public
     * interface of your driver class. The harness only provides writePacket
     * and readPacket methods with signature more suited to the testing
     * harnesses assertions.
     *
     * The "device" side gives you access to the byte stream as the real device
     * would see it. It allows to check what the driver has sent, and "reply" to
     * it. The two methods are readDataFromDriver and pushDataToDriver.
     *
     * In Boost.Test:
     *
     * <code>
     * BOOST_FIXTURE_TEST_CASE(MyTest, Fixture<MyDriver>)
     * {
     *   driver.openURI("test://");
     *   uint8_t buffer[4] = { 0, 1, 2, 3 };
     *   pushDataToDriver(buffer, buffer + 2);
     *   auto packet = readPacket();
     *   // Check that the packet matches the expected extraction
     * }
     * </code>
     *
     * In GTest:
     *
     * <code>
     * struct DriverTest : ::testing::Test, iodrivers_base::Fixture<MyDriver>
     * {
     *    Fixture()
     *    {
     *       // Optional: open here
     *       // driver.openURI("test://")
     *    }
     *
     * }
     *
     * TEST_F(DriverTest, it_handles_an_invalid_packet)
     * {
     *   MyDriver.openURI("test://");
     *   uint8_t buffer[4] = { 0, 1, 2, 3 };
     *   pushDataToDriver(buffer, buffer + 2);
     *   auto packet = readPacket();
     *   // Check that the packet matches the expected extraction
     * }
     * </code>
     *
     * In addition to this "direct interaction" mode, the harness provides an
     * "expectation" mode, in which tests are a list of expectations on the
     * messages that should be sent by the driver to the device, and canning
     * replies to these messages. It is more declarative, and allows to test
     * methods that would internally do multiple interactions with I/O.
     *
     * To use the expectations, one must first add IODRIVERS_BASE_MOCK(); to
     * the test method. Then, the request/replies are written as calls to
     * EXPECT_REPLY(expected_call, reply);
     *
     * Once all expectations are set, call the method that initiate the
     * interaction
     *
     * For instance, in both Boost.Test and GTest:
     *
     * <code>
     * IODRIVER_BASE_MOCK()
     * uint8_t exp[] = { 0, 1, 2, 3 };
     * uint8_t rep[] = { 3, 2, 1, 0 };
     * EXPECT_REPLY(vector<uint8_t>(exp, exp + 4),
     *                    vector<uint8_t>(rep, rep + 4));
     * writePacket(exp,4);
     * <code>
     */
    template<typename Driver>
    struct Fixture
    {
        typedef Driver fixture_driver_t;

        std::vector<uint8_t> packetBuffer;
        Driver driver;

        Fixture()
        {
            packetBuffer.resize(driver.MAX_PACKET_SIZE);
        }

        /** Get the underlying TestStream
         */
        TestStream* getStream()
        {
            if (!driver.getMainStream())
                driver.openTestMode();
            return dynamic_cast<TestStream*>(driver.getMainStream());
        }

        /** Read a packet from the driver and return it as an std::vector
         */
        std::vector<uint8_t> readPacket()
        {
            size_t size = driver.readPacket(packetBuffer.data(), packetBuffer.size());
            std::vector<uint8_t> packet;
            packet.insert(packet.end(), packetBuffer.begin(), packetBuffer.begin() + size);
            return packet;
        }

        /** Write data to the driver */
        void writePacket(std::vector<uint8_t> const& data)
        {
            writePacket(data.data(), data.size());
        }

        /** Write data to the driver */
        void writePacket(uint8_t const* buffer, size_t size)
        {
            driver.writePacket(buffer, size);
        }

        /** Push data to the driver "as-if" it was coming from the device
         */
        void pushDataToDriver(std::vector<uint8_t> const& data)
        {
            return getStream()->pushDataToDriver(data);
        }

        /** Helper method to allow passing any kind of uint8_t range
         *
         * <code>
         * uint8_t packet[] = { 0, 1, 2, 3 };
         * pushDataToDriver(packet, packet + sizeof(packet));
         * </code>
         */
        template<typename Iterator>
        void pushDataToDriver(Iterator begin, Iterator end)
        {
            std::vector<uint8_t> buffer(begin, end);
            pushDataToDriver(buffer);
        }

        /** Read data that the driver sent to the device
         */
        std::vector<uint8_t> readDataFromDriver()
        {
            return getStream()->readDataFromDriver();
        }

        /** Return the number of bytes currently queued in the driver's internal
         * buffer
         *
         * This is useful mainly when testing extractPacket
         *
         * <code>
         * TEST_F(DriverTest, KeepsSingleBytes)
         * {
         *   uint8_t data = 0x01;
         *   pushDataToDriver(&data, &data + 1);
         *   ASSERT_THROW(TimeoutError, readPacket());
         *   ASSERT_EQUAL(1, getQueuedBytes());
         * }
         * </code>
         */
        unsigned int getQueuedBytes() const
        {
            return driver.getStatus().queued_bytes;
        }
        void EXPECT_REPLY(std::vector<uint8_t> const& expectation, std::vector<uint8_t> const& reply)
        {
            getStream()->EXPECT_REPLY(expectation,reply);
        }

        /**
         * Check if the test has any expectation set and throw if positive.
         * It should be used to check if the test reached its end without 
         * any expecation left only
         */
        void validateExpectationsAreEmpty()
        {
            if(!getStream()->expectationsAreEmpty())
                throw TestEndsWithExpectationsLeftException();
        }

        void setMockMode(bool mode)
        {
            getStream()->setMockMode(mode);
        }

        void clearExpectations()
        {
            getStream()->clearExpectations();
        }

        class GTestMockContext;
        class BoostMockContext;
    };
}

#endif
